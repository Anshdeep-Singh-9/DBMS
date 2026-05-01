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

static int find_column_index(const table* meta, const std::string& col_name) {
    if (meta == NULL) return -1;
    for (int i = 0; i < meta->count; ++i) {
        if (col_name == meta->col[i].col_name) return i;
    }
    return -1;
}

static bool parse_qualified_column(const std::string& token,
                                   std::string& table_name,
                                   std::string& column_name) {
    std::size_t dot = token.find('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= token.size()) {
        return false;
    }
    table_name = token.substr(0, dot);
    column_name = token.substr(dot + 1);
    return !table_name.empty() && !column_name.empty();
}

static bool tuple_values_equal(const TupleValue& a, const TupleValue& b) {
    if (a.type == STORAGE_COLUMN_INT && b.type == STORAGE_COLUMN_INT) {
        return a.int_value == b.int_value;
    }
    if (a.type == STORAGE_COLUMN_VARCHAR && b.type == STORAGE_COLUMN_VARCHAR) {
        return a.string_value == b.string_value;
    }
    if (a.type == STORAGE_COLUMN_INT && b.type == STORAGE_COLUMN_VARCHAR) {
        return std::to_string(a.int_value) == b.string_value;
    }
    if (a.type == STORAGE_COLUMN_VARCHAR && b.type == STORAGE_COLUMN_INT) {
        return a.string_value == std::to_string(b.int_value);
    }
    return false;
}

static bool execute_select_join_nested(const std::vector<std::string>& target_cols,
                                       const std::string& left_table,
                                       const std::string& right_table,
                                       const std::string& left_on,
                                       const std::string& right_on,
                                       bool left_join,
                                       bool where_present,
                                       const std::string& where_column,
                                       const std::string& where_value,
                                       QueryResult* res) {
    char left_tab[MAX_NAME];
    char right_tab[MAX_NAME];
    strncpy(left_tab, left_table.c_str(), MAX_NAME - 1);
    left_tab[MAX_NAME - 1] = '\0';
    strncpy(right_tab, right_table.c_str(), MAX_NAME - 1);
    right_tab[MAX_NAME - 1] = '\0';

    if (search_table(left_tab) == 0 || search_table(right_tab) == 0) {
        if (res) {
            res->success = false;
            res->message = "Error: One or both JOIN tables do not exist.";
        }
        std::cout << "Error: One or both JOIN tables do not exist.\n";
        return false;
    }

    table* left_meta = fetch_meta_data(left_table);
    table* right_meta = fetch_meta_data(right_table);
    if (left_meta == NULL || right_meta == NULL) {
        if (res) {
            res->success = false;
            res->message = "Error: Could not load JOIN metadata.";
        }
        std::cout << "Error: Could not load JOIN metadata.\n";
        delete left_meta;
        delete right_meta;
        return false;
    }

    std::vector<ColumnSchema> left_schema;
    std::vector<ColumnSchema> right_schema;
    build_schema_from_meta(left_meta, left_schema);
    build_schema_from_meta(right_meta, right_schema);

    std::string left_on_table, left_on_col, right_on_table, right_on_col;
    if (!parse_qualified_column(left_on, left_on_table, left_on_col) ||
        !parse_qualified_column(right_on, right_on_table, right_on_col)) {
        std::cout << "Syntax Error: JOIN ON needs qualified columns like a.id = b.id\n";
        delete left_meta;
        delete right_meta;
        return false;
    }

    if (left_on_table != left_table || right_on_table != right_table) {
        std::cout << "Syntax Error: JOIN ON columns must match table names in FROM/JOIN.\n";
        delete left_meta;
        delete right_meta;
        return false;
    }

    int left_on_idx = find_column_index(left_meta, left_on_col);
    int right_on_idx = find_column_index(right_meta, right_on_col);
    if (left_on_idx < 0 || right_on_idx < 0) {
        std::cout << "Error: JOIN ON column not found in one of the tables.\n";
        delete left_meta;
        delete right_meta;
        return false;
    }

    std::vector<std::vector<TupleValue>> left_rows;
    std::vector<std::vector<TupleValue>> right_rows;
    if (!load_all_rows_using_buffer_pool(left_table, left_schema, left_rows) ||
        !load_all_rows_using_buffer_pool(right_table, right_schema, right_rows)) {
        delete left_meta;
        delete right_meta;
        return false;
    }

    std::vector<ColumnSchema> output_schema;
    std::vector<std::pair<int, int> > projections; // 0=left,1=right ; col idx
    bool select_all = (target_cols.size() == 1 && target_cols[0] == "*");

    int where_side = -1;
    int where_idx = -1;
    if (where_present) {
        std::string wt, wc;
        if (parse_qualified_column(where_column, wt, wc)) {
            if (wt == left_table) {
                where_side = 0;
                where_idx = find_column_index(left_meta, wc);
            } else if (wt == right_table) {
                where_side = 1;
                where_idx = find_column_index(right_meta, wc);
            }
        } else {
            int li = find_column_index(left_meta, where_column);
            int ri = find_column_index(right_meta, where_column);
            if (li >= 0 && ri >= 0) {
                std::cout << "Error: Ambiguous WHERE column '" << where_column
                          << "'. Use qualified name.\n";
                delete left_meta;
                delete right_meta;
                return false;
            }
            if (li >= 0) {
                where_side = 0;
                where_idx = li;
            } else if (ri >= 0) {
                where_side = 1;
                where_idx = ri;
            }
        }

        if (where_idx < 0) {
            std::cout << "Error: WHERE column not found in JOIN tables.\n";
            delete left_meta;
            delete right_meta;
            return false;
        }
    }

    if (select_all) {
        for (int i = 0; i < left_meta->count; ++i) {
            ColumnSchema col = left_schema[i];
            col.name = left_table + "." + col.name;
            output_schema.push_back(col);
            projections.push_back(std::make_pair(0, i));
        }
        for (int i = 0; i < right_meta->count; ++i) {
            ColumnSchema col = right_schema[i];
            col.name = right_table + "." + col.name;
            output_schema.push_back(col);
            projections.push_back(std::make_pair(1, i));
        }
    } else {
        for (std::size_t i = 0; i < target_cols.size(); ++i) {
            std::string tname, cname;
            if (!parse_qualified_column(target_cols[i], tname, cname)) {
                std::cout << "Error: In JOIN select list, use qualified columns like table.column\n";
                delete left_meta;
                delete right_meta;
                return false;
            }

            if (tname == left_table) {
                int idx = find_column_index(left_meta, cname);
                if (idx < 0) {
                    std::cout << "Error: Column " << target_cols[i] << " not found.\n";
                    delete left_meta;
                    delete right_meta;
                    return false;
                }
                ColumnSchema col = left_schema[idx];
                col.name = target_cols[i];
                output_schema.push_back(col);
                projections.push_back(std::make_pair(0, idx));
            } else if (tname == right_table) {
                int idx = find_column_index(right_meta, cname);
                if (idx < 0) {
                    std::cout << "Error: Column " << target_cols[i] << " not found.\n";
                    delete left_meta;
                    delete right_meta;
                    return false;
                }
                ColumnSchema col = right_schema[idx];
                col.name = target_cols[i];
                output_schema.push_back(col);
                projections.push_back(std::make_pair(1, idx));
            } else {
                std::cout << "Error: Unknown table prefix in " << target_cols[i] << "\n";
                delete left_meta;
                delete right_meta;
                return false;
            }
        }
    }

    std::vector<std::vector<TupleValue>> output_rows;
    for (std::size_t i = 0; i < left_rows.size(); ++i) {
        bool matched_any = false;
        for (std::size_t j = 0; j < right_rows.size(); ++j) {
            if (!tuple_values_equal(left_rows[i][left_on_idx], right_rows[j][right_on_idx])) {
                continue;
            }

            if (where_present) {
                TupleValue expected = TupleValue::FromVarchar(where_value);
                if (where_side == 0 && left_rows[i][where_idx].type == STORAGE_COLUMN_INT) {
                    try {
                        expected = TupleValue::FromInt(std::stoi(where_value));
                    } catch (...) {
                        continue;
                    }
                }
                if (where_side == 1 && right_rows[j][where_idx].type == STORAGE_COLUMN_INT) {
                    try {
                        expected = TupleValue::FromInt(std::stoi(where_value));
                    } catch (...) {
                        continue;
                    }
                }

                bool pass = (where_side == 0)
                                ? tuple_values_equal(left_rows[i][where_idx], expected)
                                : tuple_values_equal(right_rows[j][where_idx], expected);
                if (!pass) continue;
            }

            std::vector<TupleValue> out;
            for (std::size_t p = 0; p < projections.size(); ++p) {
                if (projections[p].first == 0) out.push_back(left_rows[i][projections[p].second]);
                else out.push_back(right_rows[j][projections[p].second]);
            }
            output_rows.push_back(out);
            matched_any = true;
        }

        if (left_join && !matched_any) {
            if (where_present && where_side == 0) {
                TupleValue expected = TupleValue::FromVarchar(where_value);
                if (left_rows[i][where_idx].type == STORAGE_COLUMN_INT) {
                    try {
                        expected = TupleValue::FromInt(std::stoi(where_value));
                    } catch (...) {
                        continue;
                    }
                }
                if (!tuple_values_equal(left_rows[i][where_idx], expected)) continue;
            } else if (where_present && where_side == 1) {
                continue;
            }

            std::vector<TupleValue> out;
            for (std::size_t p = 0; p < projections.size(); ++p) {
                if (projections[p].first == 0) {
                    out.push_back(left_rows[i][projections[p].second]);
                } else {
                    const ColumnSchema& col = right_schema[projections[p].second];
                    if (col.type == STORAGE_COLUMN_INT) out.push_back(TupleValue::FromInt(0));
                    else out.push_back(TupleValue::FromVarchar(""));
                }
            }
            output_rows.push_back(out);
        }
    }

    if (res) {
        res->is_select = true;
        res->schema = output_schema;
        res->rows = output_rows;
        res->strategy = "Nested Loop Join";
    }

    print_table(output_schema, output_rows);

    delete left_meta;
    delete right_meta;
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
                    const std::vector<std::string>& target_cols, QueryResult* res) {
    char tab[MAX_NAME];
    strncpy(tab, tab_name.c_str(), MAX_NAME - 1);
    tab[MAX_NAME - 1] = '\0';

    if (search_table(tab) == 0) {
        if (res) {
            res->success = false;
            res->message = "Table \"" + tab_name + "\" does not exist.";
        }
        std::cout << "\nTable \"" << tab_name << "\" does not exist.\n";
        return;
    }

    table* meta = fetch_meta_data(tab_name);
    if (meta == NULL) {
        if (res) {
            res->success = false;
            res->message = "Error: Could not load table metadata.";
        }
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
                if (res) {
                    res->success = false;
                    res->message = "Error: Column '" + col_name + "' does not exist in table.";
                }
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
        if (res) {
            res->success = false;
            res->message = "Error: Failed to load rows.";
        }
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

    if (res) {
        res->is_select = true;
        res->schema = output_schema;
        res->rows = output_rows;
        res->strategy = "Linear Scan (No WHERE clause)";
    }

    print_table(output_schema, output_rows);

    delete meta;
}
void process_select(std::vector<std::string>& token_vector, QueryResult* res) {
    if (token_vector.size() < 4 || token_vector[0] != "select") {
        if (res) {
            res->success = false;
            res->message = "Syntax Error: Invalid SELECT statement.";
        }
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
        if (res) {
            res->success = false;
            res->message = "Syntax Error: Missing FROM clause or table name.";
        }
        std::cout << "Syntax Error: Missing FROM clause or table name.\n";
        return;
    }

    table_name = token_vector[from_index + 1];

    int join_kw_index = from_index + 2;
    bool left_join = false;
    if ((int)token_vector.size() > join_kw_index &&
        (token_vector[join_kw_index] == "left" || token_vector[join_kw_index] == "inner")) {
        left_join = (token_vector[join_kw_index] == "left");
        join_kw_index++;
    }

    if ((int)token_vector.size() > join_kw_index && token_vector[join_kw_index] == "join") {
        if ((int)token_vector.size() < join_kw_index + 6) {
            std::cout << "Syntax Error: Incomplete JOIN.\n";
            std::cout << "Use: SELECT ... FROM t1 JOIN t2 ON t1.col = t2.col;\n";
            return;
        }

        std::string right_table = token_vector[join_kw_index + 1];
        if (token_vector[join_kw_index + 2] != "on") {
            std::cout << "Syntax Error: Missing ON in JOIN clause.\n";
            return;
        }

        std::string left_on = token_vector[join_kw_index + 3];
        std::string op = token_vector[join_kw_index + 4];
        std::string right_on = token_vector[join_kw_index + 5];

        if (op != "=") {
            std::cout << "Syntax Error: Only '=' is supported in JOIN ON.\n";
            return;
        }

        bool where_present = false;
        std::string where_col, where_val;
        int where_index = join_kw_index + 6;
        if ((int)token_vector.size() > where_index) {
            if (token_vector[where_index] != "where") {
                std::cout << "Syntax Error: Only WHERE is allowed after JOIN ON clause.\n";
                return;
            }
            if ((int)token_vector.size() < where_index + 4) {
                std::cout << "Syntax Error: Incomplete WHERE after JOIN.\n";
                return;
            }
            if (token_vector[where_index + 2] != "=") {
                std::cout << "Syntax Error: Only '=' is supported in WHERE.\n";
                return;
            }

            where_present = true;
            where_col = token_vector[where_index + 1];
            for (int i = where_index + 3; i < (int)token_vector.size(); ++i) {
                if (!where_val.empty()) where_val += " ";
                where_val += token_vector[i];
            }
            if (where_val.empty()) {
                std::cout << "Syntax Error: Missing WHERE value.\n";
                return;
            }
        }

        execute_select_join_nested(target_cols, table_name, right_table, left_on, right_on,
                                   left_join,
                                   where_present, where_col, where_val, res);
        return;
    }

    WhereClause where_clause;
    int where_start = from_index + 2;

    if ((int)token_vector.size() > where_start &&
        token_vector[where_start] == "where") {

        if ((int)token_vector.size() < where_start + 4) {
            if (res) {
                res->success = false;
                res->message = "Syntax Error: Incomplete WHERE clause.";
            }
            std::cout << "Syntax Error: Incomplete WHERE clause.\n";
            std::cout << "Usage: WHERE <column> = <value>\n";
            return;
        }

        std::string op = token_vector[where_start + 2];

        if (op != "=") {
            if (res) {
                res->success = false;
                res->message = "Syntax Error: Only '=' is supported in WHERE clause.";
            }
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
            if (res) {
                res->success = false;
                res->message = "Syntax Error: Missing value in WHERE clause.";
            }
            std::cout << "Syntax Error: Missing value in WHERE clause.\n";
            return;
        }
    }

    if (where_clause.present) {
        execute_select_where(table_name, target_cols, where_clause, res);
    } else {
        execute_select(table_name, target_cols, res);
    }
}
