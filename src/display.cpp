#include "display.h"
#include "buffer_pool_manager.h"
#include "data_page.h"
#include "disk_manager.h"
#include "file_handler.h"
#include "tuple_serializer.h"
#include "where.h"

#include <iomanip>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>

#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define BLUE    "\033[34m"
#define WHITE   "\033[97m"
#define GREEN   "\033[32m"
void print_table(const std::vector<ColumnSchema>& schema,
                 const std::vector<std::vector<TupleValue>>& table_data) {
    if (schema.empty()) {
        std::cout << "\n(No columns to display)\n";
        return;
    }

    std::vector<int> widths(schema.size(), 0);

    for (size_t i = 0; i < schema.size(); i++) {
        widths[i] = std::max(12, (int)schema[i].name.length());
    }

    for (const auto& row : table_data) {
        for (size_t i = 0; i < row.size() && i < widths.size(); i++) {
            int len = 0;

            if (row[i].type == STORAGE_COLUMN_INT) {
                len = std::to_string(row[i].int_value).length();
            } else {
                len = row[i].string_value.length();
            }

            widths[i] = std::max(widths[i], len);
        }
    }

    for (size_t i = 0; i < widths.size(); i++) {
        widths[i] += 4;
    }

    auto print_border = [&]() {
        std::cout << BLUE << "+";
        for (int w : widths) {
            std::cout << std::string(w, '-') << "+";
        }
        std::cout << RESET << "\n";
    };

    auto print_empty_message = [&]() {
        std::cout << BLUE << "|" << RESET;

        int total_width = 0;
        for (int w : widths) total_width += w;
        total_width += (int)widths.size() - 1;

        std::string msg = " Table is empty ";
        int left_padding = std::max(0, (total_width - (int)msg.length()) / 2);
        int right_padding = std::max(0, total_width - left_padding - (int)msg.length());

        std::cout << std::string(left_padding, ' ')
                  << WHITE << msg << RESET
                  << std::string(right_padding, ' ');

        std::cout << BLUE << "|" << RESET << "\n";
    };

    std::cout << "\n";

    print_border();

    std::cout << BLUE << "|" << RESET;
    for (size_t i = 0; i < schema.size(); i++) {
        std::cout << BOLD << WHITE
                  << " " << std::left << std::setw(widths[i] - 1) << schema[i].name
                  << RESET << BLUE << "|" << RESET;
    }
    std::cout << "\n";

    print_border();

    if (table_data.empty()) {
        print_empty_message();
    } else {
        for (const auto& row : table_data) {
            std::cout << BLUE << "|" << RESET;

            for (size_t i = 0; i < schema.size(); i++) {
                std::string value;

                if (i < row.size()) {
                    if (row[i].type == STORAGE_COLUMN_INT) {
                        value = std::to_string(row[i].int_value);
                    } else {
                        value = row[i].string_value;
                    }
                } else {
                    value = "";
                }

                std::cout << WHITE
                          << " " << std::left << std::setw(widths[i] - 1) << value
                          << RESET << BLUE << "|" << RESET;
            }

            std::cout << "\n";
        }
    }

    print_border();

    std::cout << BOLD << WHITE
              << "Total rows: " << table_data.size()
              << RESET << "\n\n";
}

int search_table(char tab_name[]) {
    char str[MAX_NAME + 1];
    strcpy(str, "grep -Fxq ");
    strcat(str, tab_name);
    strcat(str, " ./table/table_list");

    int x = system(str);

    if (x == 0) {
        return 1;
    }

    return 0;
}

static bool build_schema_from_meta(table* meta, std::vector<ColumnSchema>& schema) {
    if (meta == NULL) {
        return false;
    }

    schema.clear();

    for (int i = 0; i < meta->count; i++) {
        ColumnSchema col_schema(
            meta->col[i].col_name,
            meta->col[i].type == INT ? STORAGE_COLUMN_INT : STORAGE_COLUMN_VARCHAR,
            meta->col[i].size
        );

        schema.push_back(col_schema);
    }

    return true;
}

static bool load_all_rows_using_buffer_pool(const std::string& tab_name,
                                            const std::vector<ColumnSchema>& full_schema,
                                            std::vector<std::vector<TupleValue>>& rows) {
    rows.clear();

    std::string data_path = "table/" + tab_name + "/data.dat";

    DiskManager data_disk(data_path);
    if (!data_disk.open_or_create()) {
        std::cout << "Error: Could not open data file.\n";
        return false;
    }

    BufferPoolManager buffer_pool(4, &data_disk);

    for (uint32_t i = 0; i < data_disk.page_count(); ++i) {
        char* frame_data = buffer_pool.fetch_page(i);

        if (frame_data == NULL) {
            continue;
        }

        DataPage page;
        page.load_from_buffer(frame_data, STORAGE_PAGE_SIZE);

        for (uint16_t slot_id = 0; slot_id < page.slot_count(); ++slot_id) {
            std::vector<char> tuple_data;

            if (!page.read_tuple(slot_id, tuple_data)) {
                continue;
            }

            std::vector<TupleValue> values;

            if (TupleSerializer::deserialize(full_schema, tuple_data, values)) {
                rows.push_back(values);
            }
        }

        buffer_pool.unpin_page(i, false);
    }

    return true;
}

void display() {
    char tab[MAX_NAME];

    std::cout << "Enter table name: ";
    std::cin >> tab;

    if (search_table(tab) == 0) {
        std::cout << "\nTable \"" << tab << "\" does not exist.\n";
        return;
    }

    table* meta = fetch_meta_data(tab);
    if (meta == NULL) {
        std::cout << "Error: Could not load table metadata.\n";
        return;
    }

    std::vector<ColumnSchema> full_schema;
    build_schema_from_meta(meta, full_schema);

    std::vector<std::vector<TupleValue>> rows;
    load_all_rows_using_buffer_pool(tab, full_schema, rows);

    std::cout << "\nDisplaying contents for table: " << tab << "\n";
    print_table(full_schema, rows);

    delete meta;
}

void show_tables() {
    char name[MAX_NAME];

    std::vector<ColumnSchema> schema;
    schema.push_back(ColumnSchema("Table Name", STORAGE_COLUMN_VARCHAR, MAX_NAME));

    std::vector<std::vector<TupleValue>> rows;

    FilePtr fp = fopen("./table/table_list", "r");

    if (fp) {
        while (fscanf(fp, "%s", name) != EOF) {
            std::vector<TupleValue> row;
            row.push_back(TupleValue::FromVarchar(name));
            rows.push_back(row);
        }

        fclose(fp);
    }

    std::cout << "\n" << GREEN << BOLD << "LIST OF TABLES" << RESET << "\n";
    print_table(schema, rows);
}

void display_meta_data() {
    std::string name;

    std::cout << "Enter the name of table: ";
    std::cin >> name;

    if (name.empty()) {
        std::cout << "ERROR! No name entered.\n";
        return;
    }

    table* t_ptr = fetch_meta_data(name);

    if (t_ptr == NULL) {
        std::cout << "ERROR! Table not found or failed to read metadata.\n";
        return;
    }

    std::cout << "\n" << BOLD << BLUE << "TABLE METADATA" << RESET << "\n";
    std::cout << BLUE << "----------------------------------------" << RESET << "\n";
    std::cout << BOLD << "Table Name   : " << RESET << t_ptr->name << "\n";
    std::cout << BOLD << "Max Row Size : " << RESET << t_ptr->size << "\n";
    std::cout << BOLD << "Columns      : " << RESET << t_ptr->count << "\n";
    std::cout << BLUE << "----------------------------------------" << RESET << "\n\n";

    std::vector<ColumnSchema> schema;
    schema.push_back(ColumnSchema("Column Name", STORAGE_COLUMN_VARCHAR, MAX_NAME));
    schema.push_back(ColumnSchema("Type", STORAGE_COLUMN_VARCHAR, 20));
    schema.push_back(ColumnSchema("Size", STORAGE_COLUMN_INT, sizeof(int)));

    std::vector<std::vector<TupleValue>> rows;

    for (int i = 0; i < t_ptr->count; i++) {
        std::vector<TupleValue> row;

        row.push_back(TupleValue::FromVarchar(t_ptr->col[i].col_name));
        row.push_back(TupleValue::FromVarchar(t_ptr->col[i].type == INT ? "INT" : "VARCHAR"));
        row.push_back(TupleValue::FromInt(t_ptr->col[i].size));

        rows.push_back(row);
    }

    print_table(schema, rows);

    delete t_ptr;
}

void execute_select(const std::string& tab_name,
                    const std::vector<std::string>& target_cols) {
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

    bool select_all = (target_cols.size() == 1 && target_cols[0] == "*");
    std::vector<int> selected_indices;

    if (select_all) {
        for (int i = 0; i < meta->count; i++) {
            selected_indices.push_back(i);
        }
    } else {
        for (const auto& col_name : target_cols) {
            bool found = false;

            for (int i = 0; i < meta->count; i++) {
                if (col_name == meta->col[i].col_name) {
                    selected_indices.push_back(i);
                    found = true;
                    break;
                }
            }

            if (!found) {
                std::cout << "Error: Column '" << col_name << "' does not exist in table.\n";
                delete meta;
                return;
            }
        }
    }

    std::vector<ColumnSchema> full_schema;
    build_schema_from_meta(meta, full_schema);

    std::vector<ColumnSchema> output_schema;

    for (int idx : selected_indices) {
        output_schema.push_back(full_schema[idx]);
    }

    std::vector<std::vector<TupleValue>> all_rows;

    if (!load_all_rows_using_buffer_pool(tab_name, full_schema, all_rows)) {
        delete meta;
        return;
    }

    std::vector<std::vector<TupleValue>> output_rows;

    for (const auto& row : all_rows) {
        std::vector<TupleValue> selected_row;

        for (int idx : selected_indices) {
            selected_row.push_back(row[idx]);
        }

        output_rows.push_back(selected_row);
    }

    print_table(output_schema, output_rows);

    delete meta;
}
void process_select(std::vector<std::string>& token_vector) {
    if (token_vector.size() < 4 || token_vector[0] != "select") {
        std::cout << "Syntax Error: Invalid SELECT statement.\n";
        return;
    }

    std::vector<std::string> target_cols;
    std::string table_name = "";
    int from_index = -1;

    for (size_t i = 1; i < token_vector.size(); i++) {
        if (token_vector[i] == "from") {
            from_index = static_cast<int>(i);
            break;
        } else {
            target_cols.push_back(token_vector[i]);
        }
    }

    if (from_index == -1 || (int)token_vector.size() <= from_index + 1) {
        std::cout << "Syntax Error: Missing FROM clause or table name.\n";
        return;
    }

    table_name = token_vector[from_index + 1];

    WhereClause where_clause;
    int where_start = from_index + 2;

    if ((int)token_vector.size() > where_start &&
        token_vector[where_start] == "where") {

        if ((int)token_vector.size() < where_start + 4) {
            std::cout << "Syntax Error: Incomplete WHERE clause.\n";
            std::cout << "Usage: WHERE <column> = <value>\n";
            return;
        }

        std::string op = token_vector[where_start + 2];

        if (op != "=") {
            std::cout << "Syntax Error: Only '=' is supported in WHERE clause.\n";
            return;
        }

        where_clause.present = true;
        where_clause.column = token_vector[where_start + 1];
        where_clause.op = op;

        // Join all remaining tokens after '=' as the WHERE value.
        // This allows values like: Aditya Sirsalkar
        where_clause.value = "";
        for (int i = where_start + 3; i < (int)token_vector.size(); i++) {
            if (!where_clause.value.empty()) {
                where_clause.value += " ";
            }
            where_clause.value += token_vector[i];
        }

        if (where_clause.value.empty()) {
            std::cout << "Syntax Error: Missing value in WHERE clause.\n";
            return;
        }
    }

    if (where_clause.present) {
        execute_select_where(table_name, target_cols, where_clause);
    } else {
        execute_select(table_name, target_cols);
    }
}