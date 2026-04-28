#include "where.h"
#include "BPtree.h"
#include "data_page.h"
#include "disk_manager.h"
#include "display.h"
#include "file_handler.h"
#include "tuple_serializer.h"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>

#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define BLUE    "\033[34m"
#define WHITE   "\033[97m"

static std::string remove_quotes_local(std::string s) {
    while (!s.empty() && isspace((unsigned char)s.front())) {
        s.erase(s.begin());
    }

    while (!s.empty() && isspace((unsigned char)s.back())) {
        s.pop_back();
    }

    if (s.size() >= 2) {
        if ((s.front() == '\'' && s.back() == '\'') ||
            (s.front() == '"' && s.back() == '"')) {
            return s.substr(1, s.size() - 2);
        }
    }

    return s;
}

static void print_result_table(const std::vector<ColumnSchema>& schema,
                               const std::vector<std::vector<TupleValue>>& table_data) {
    if (schema.empty()) {
        std::cout << "(No columns to display)\n";
        return;
    }

    std::cout << "\n" << BOLD << BLUE;

    for (const auto& col : schema) {
        std::cout << std::left << std::setw(20) << col.name;
    }

    std::cout << RESET << "\n";

    std::cout << BLUE;
    for (size_t i = 0; i < schema.size(); ++i) {
        std::cout << "--------------------";
    }
    std::cout << RESET << "\n";

    if (table_data.empty()) {
        std::cout << WHITE << "(No matching rows found)\n" << RESET;
    } else {
        std::cout << WHITE;

        for (const auto& row : table_data) {
            for (const auto& val : row) {
                if (val.type == STORAGE_COLUMN_INT) {
                    std::cout << std::left << std::setw(20) << val.int_value;
                } else {
                    std::cout << std::left << std::setw(20) << val.string_value;
                }
            }

            std::cout << "\n";
        }

        std::cout << RESET;
    }

    std::cout << BLUE;
    for (size_t i = 0; i < schema.size(); ++i) {
        std::cout << "--------------------";
    }
    std::cout << RESET << "\n";

    std::cout << BOLD << WHITE << "Total rows: " << table_data.size() << RESET << "\n\n";
}

static std::vector<TupleValue> project_row(const std::vector<TupleValue>& values,
                                           const std::vector<int>& col_indices_to_print) {
    std::vector<TupleValue> projected;

    for (int idx : col_indices_to_print) {
        projected.push_back(values[idx]);
    }

    return projected;
}

static bool row_matches_where(const std::vector<TupleValue>& values,
                              int where_col_idx,
                              const WhereClause& where) {
    const TupleValue& cell = values[where_col_idx];
    std::string where_value = remove_quotes_local(where.value);

    if (cell.type == STORAGE_COLUMN_INT) {
        try {
            int target = std::stoi(where_value);
            return cell.int_value == target;
        } catch (...) {
            return false;
        }
    }

    return cell.string_value == where_value;
}

static void search_via_bptree(const std::string& tab_name,
                              const std::vector<ColumnSchema>& schema,
                              const std::vector<int>& col_indices_to_print,
                              const std::vector<ColumnSchema>& output_schema,
                              const WhereClause& where) {
    std::cout << "\n[Search Strategy: B+ Tree Point Lookup on Primary Key]\n";

    int pk_value;

    try {
        pk_value = std::stoi(remove_quotes_local(where.value));
    } catch (...) {
        std::cout << "Error: WHERE value '" << where.value
                  << "' cannot be parsed as INT for primary key column '"
                  << where.column << "'.\n";
        return;
    }

    BPtree index(tab_name.c_str());
    RID rid = index.search(pk_value);

    std::vector<std::vector<TupleValue>> output_rows;

    if (rid.page_id != INVALID_PAGE_ID) {
        std::string data_path = "table/" + tab_name + "/data.dat";

        DiskManager data_disk(data_path);
        if (!data_disk.open_or_create()) {
            std::cout << "Error: Could not open data file.\n";
            return;
        }

        char page_buffer[STORAGE_PAGE_SIZE];

        if (!data_disk.read_page(rid.page_id, page_buffer)) {
            std::cout << "Error: Could not read page " << rid.page_id << ".\n";
            return;
        }

        DataPage page;
        page.load_from_buffer(page_buffer, STORAGE_PAGE_SIZE);

        std::vector<char> tuple_data;

        if (!page.read_tuple(rid.slot_id, tuple_data)) {
            std::cout << "Error: Could not read slot " << rid.slot_id << " from page "
                      << rid.page_id << ".\n";
            return;
        }

        std::vector<TupleValue> values;

        if (!TupleSerializer::deserialize(schema, tuple_data, values)) {
            std::cout << "Error: Failed to deserialize tuple.\n";
            return;
        }

        output_rows.push_back(project_row(values, col_indices_to_print));
    }

    print_result_table(output_schema, output_rows);
}

static void search_via_linear_scan(const std::string& tab_name,
                                   const std::vector<ColumnSchema>& schema,
                                   const std::vector<int>& col_indices_to_print,
                                   const std::vector<ColumnSchema>& output_schema,
                                   const WhereClause& where,
                                   int where_col_idx) {
    std::cout << "\n[Search Strategy: Linear Scan]\n";

    std::string data_path = "table/" + tab_name + "/data.dat";

    DiskManager data_disk(data_path);
    if (!data_disk.open_or_create()) {
        std::cout << "Error: Could not open data file.\n";
        return;
    }

    std::vector<std::vector<TupleValue>> output_rows;

    for (uint32_t i = 0; i < data_disk.page_count(); ++i) {
        char page_buffer[STORAGE_PAGE_SIZE];

        if (!data_disk.read_page(i, page_buffer)) {
            continue;
        }

        DataPage page;
        page.load_from_buffer(page_buffer, STORAGE_PAGE_SIZE);

        for (uint16_t slot_id = 0; slot_id < page.slot_count(); ++slot_id) {
            std::vector<char> tuple_data;

            if (!page.read_tuple(slot_id, tuple_data)) {
                continue;
            }

            std::vector<TupleValue> values;

            if (!TupleSerializer::deserialize(schema, tuple_data, values)) {
                continue;
            }

            if (row_matches_where(values, where_col_idx, where)) {
                output_rows.push_back(project_row(values, col_indices_to_print));
            }
        }
    }

    print_result_table(output_schema, output_rows);
}

void execute_select_where(const std::string& tab_name,
                          const std::vector<std::string>& target_cols,
                          const WhereClause& where) {
    char tab[MAX_NAME];
    strncpy(tab, tab_name.c_str(), MAX_NAME - 1);
    tab[MAX_NAME - 1] = '\0';

    if (search_table(tab) == 0) {
        std::cout << "\nTable \"" << tab_name << "\" does not exist.\n";
        return;
    }

    table* meta = fetch_meta_data(tab_name);

    if (meta == NULL) {
        std::cout << "Error: Could not load table metadata.\n";
        return;
    }

    int where_col_idx = -1;

    for (int i = 0; i < meta->count; i++) {
        if (where.column == meta->col[i].col_name) {
            where_col_idx = i;
            break;
        }
    }

    if (where_col_idx == -1) {
        std::cout << "Error: Column '" << where.column
                  << "' does not exist in table '" << tab_name << "'.\n";
        delete meta;
        return;
    }

    bool select_all = (target_cols.size() == 1 && target_cols[0] == "*");
    std::vector<int> col_indices_to_print;

    if (select_all) {
        for (int i = 0; i < meta->count; i++) {
            col_indices_to_print.push_back(i);
        }
    } else {
        for (const auto& t_col : target_cols) {
            bool found = false;

            for (int i = 0; i < meta->count; i++) {
                if (t_col == meta->col[i].col_name) {
                    col_indices_to_print.push_back(i);
                    found = true;
                    break;
                }
            }

            if (!found) {
                std::cout << "Error: Column '" << t_col << "' does not exist.\n";
                delete meta;
                return;
            }
        }
    }

    std::vector<ColumnSchema> schema;

    for (int i = 0; i < meta->count; i++) {
        schema.push_back(ColumnSchema(
            meta->col[i].col_name,
            meta->col[i].type == INT ? STORAGE_COLUMN_INT : STORAGE_COLUMN_VARCHAR,
            meta->col[i].size
        ));
    }

    std::vector<ColumnSchema> output_schema;

    for (int idx : col_indices_to_print) {
        output_schema.push_back(schema[idx]);
    }

    bool is_pk_column = (where_col_idx == 0);
    bool pk_is_int = (meta->col[0].type == INT);

    if (is_pk_column && pk_is_int) {
        search_via_bptree(tab_name, schema, col_indices_to_print, output_schema, where);
    } else {
        search_via_linear_scan(tab_name, schema, col_indices_to_print,
                               output_schema, where, where_col_idx);
    }

    delete meta;
}