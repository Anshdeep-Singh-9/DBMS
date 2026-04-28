#include "parser.h"
#include "display.h"
#include "file_handler.h"
#include "insert.h"
#include "tuple_serializer.h"
#include "update.h"

#include <cstring>
#include <iostream>
#include <sstream>
#include <cctype>
#include <vector>
#include <fstream>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

extern void execute_create_query(string table_name, vector<pair<string, string>> cols);
extern void insert_command(char tname[], const vector<TupleValue>& values, const vector<ColumnSchema>& schema);

string trim_string(string s) {
    while (!s.empty() && isspace((unsigned char)s.front())) {
        s.erase(s.begin());
    }

    while (!s.empty() && isspace((unsigned char)s.back())) {
        s.pop_back();
    }

    return s;
}

string to_lower_string(string s) {
    for (char &c : s) {
        c = static_cast<char>(tolower((unsigned char)c));
    }

    return s;
}

string remove_quotes(string s) {
    s = trim_string(s);

    if (s.size() >= 2) {
        if ((s.front() == '\'' && s.back() == '\'') ||
            (s.front() == '"' && s.back() == '"')) {
            return s.substr(1, s.size() - 2);
        }
    }

    return s;
}

string keyword_lower_copy(string q) {
    bool in_single = false;
    bool in_double = false;

    for (int i = 0; i < (int)q.size(); i++) {
        if (q[i] == '\'' && !in_double) {
            in_single = !in_single;
        }
        else if (q[i] == '"' && !in_single) {
            in_double = !in_double;
        }
        else if (!in_single && !in_double) {
            q[i] = static_cast<char>(tolower((unsigned char)q[i]));
        }
    }

    return q;
}

vector<string> split_values(string s) {
    vector<string> values;
    string current = "";
    bool in_single = false;
    bool in_double = false;

    for (char c : s) {
        if (c == '\'' && !in_double) {
            in_single = !in_single;
            current += c;
        }
        else if (c == '"' && !in_single) {
            in_double = !in_double;
            current += c;
        }
        else if (c == ',' && !in_single && !in_double) {
            values.push_back(trim_string(current));
            current = "";
        }
        else {
            current += c;
        }
    }

    if (!current.empty()) {
        values.push_back(trim_string(current));
    }

    return values;
}

void push_select_token(vector<string>& token_vector, string token) {
    token = trim_string(token);
    if (token.empty()) return;

    string lower = to_lower_string(token);

    if (lower == "select" || lower == "from" || lower == "where") {
        token_vector.push_back(lower);
    }
    else {
        token_vector.push_back(remove_quotes(token));
    }
}

void tokenize_select(char query[]) {
    vector<string> token_vector;
    string current_token = "";
    bool in_single = false;
    bool in_double = false;

    for (int i = 0; query[i] != '\0'; i++) {
        char c = query[i];

        if (c == '\'' && !in_double) {
            in_single = !in_single;
            current_token += c;
        }
        else if (c == '"' && !in_single) {
            in_double = !in_double;
            current_token += c;
        }
        else if (!in_single && !in_double &&
                 (c == ' ' || c == ',' || c == ';' || c == '\n')) {
            push_select_token(token_vector, current_token);
            current_token = "";
        }
        else {
            if (c != '\n') {
                current_token += c;
            }
        }
    }

    push_select_token(token_vector, current_token);

    process_select(token_vector);
}

void tokenize_create(char query[]) {
    string q(query);
    string q_lower = keyword_lower_copy(q);

    size_t start = q.find('(');
    size_t end = q.rfind(')');

    if (start == string::npos || end == string::npos || end <= start) {
        cout << "Syntax Error: Invalid CREATE TABLE format. Missing parentheses.\n";
        return;
    }

    string before_paren = trim_string(q.substr(0, start));
    stringstream ss(before_paren);

    vector<string> words;
    string word;

    while (ss >> word) {
        words.push_back(word);
    }

    if (words.size() != 3 ||
        to_lower_string(words[0]) != "create" ||
        to_lower_string(words[1]) != "table") {
        cout << "Syntax Error: Use CREATE TABLE table_name (...);\n";
        return;
    }

    string table_name = words[2];

    string cols_str = q.substr(start + 1, end - start - 1);
    vector<pair<string, string>> columns;

    stringstream col_stream(cols_str);
    string col_def;

    while (getline(col_stream, col_def, ',')) {
        stringstream def_stream(trim_string(col_def));
        string col_name, col_type;

        if (def_stream >> col_name >> col_type) {
            columns.push_back({col_name, col_type});
        }
    }

    execute_create_query(table_name, columns);
}

void tokenize_insert(char query[]) {
    string q(query);
    string q_lower = keyword_lower_copy(q);

    size_t values_pos = q_lower.find(" values ");
    if (values_pos == string::npos) {
        cout << "Syntax Error: Invalid INSERT statement. Missing VALUES keyword.\n";
        return;
    }

    string before_values = trim_string(q.substr(0, values_pos));
    stringstream ss(before_values);

    string insert_word, into_word, table_name;
    ss >> insert_word >> into_word >> table_name;

    if (to_lower_string(insert_word) != "insert" ||
        to_lower_string(into_word) != "into" ||
        table_name.empty()) {
        cout << "Syntax Error: Use INSERT INTO table_name VALUES (...);\n";
        return;
    }

    size_t open_pos = q.find('(', values_pos);
    size_t close_pos = q.rfind(')');

    if (open_pos == string::npos || close_pos == string::npos || close_pos <= open_pos) {
        cout << "Syntax Error: INSERT values must be inside parentheses.\n";
        return;
    }

    string values_str = q.substr(open_pos + 1, close_pos - open_pos - 1);
    vector<string> raw_values = split_values(values_str);

    table* meta = fetch_meta_data(table_name);
    if (meta == NULL) {
        cout << "ERROR: Table '" << table_name << "' does not exist or metadata could not be loaded.\n";
        return;
    }

    if ((int)raw_values.size() != meta->count) {
        cout << "ERROR: Column count mismatch.\n";
        cout << "Expected " << meta->count << " values, received " << raw_values.size() << ".\n";
        delete meta;
        return;
    }

    vector<ColumnSchema> schema;
    vector<TupleValue> values;

    for (int i = 0; i < meta->count; i++) {
        ColumnSchema col_schema(
            meta->col[i].col_name,
            meta->col[i].type == INT ? STORAGE_COLUMN_INT : STORAGE_COLUMN_VARCHAR,
            meta->col[i].size
        );

        schema.push_back(col_schema);

        string value = remove_quotes(raw_values[i]);

        if (meta->col[i].type == INT) {
            try {
                size_t used = 0;
                int number = stoi(value, &used);

                if (used != value.size()) {
                    cout << "ERROR: Invalid INT value for column '" << meta->col[i].col_name << "'.\n";
                    delete meta;
                    return;
                }

                values.push_back(TupleValue::FromInt(number));
            } catch (...) {
                cout << "ERROR: Invalid INT value for column '" << meta->col[i].col_name << "'.\n";
                delete meta;
                return;
            }
        }
        else if (meta->col[i].type == VARCHAR) {
            if ((int)value.size() > meta->col[i].size) {
                cout << "ERROR: Value too long for column '" << meta->col[i].col_name << "'.\n";
                cout << "Max allowed size: " << meta->col[i].size << "\n";
                delete meta;
                return;
            }

            values.push_back(TupleValue::FromVarchar(value));
        }
    }

    char tname[MAX_NAME];
    strncpy(tname, table_name.c_str(), MAX_NAME - 1);
    tname[MAX_NAME - 1] = '\0';

    insert_command(tname, values, schema);

    delete meta;
}

void tokenize_show(char query[]) {
    string q(query);

    while (!q.empty() && (q.back() == ';' || q.back() == '\n' || q.back() == ' ')) {
        q.pop_back();
    }

    q = trim_string(q);
    string q_lower = to_lower_string(q);

    if (q_lower == "show tables") {
        show_tables();
    } else {
        cout << "Syntax Error: Supported SHOW syntax is:\n";
        cout << "SHOW TABLES;\n";
    }
}

void tokenize_drop(char query[]) {
    string q(query);

    while (!q.empty() && (q.back() == ';' || q.back() == '\n' || q.back() == ' ')) {
        q.pop_back();
    }

    q = trim_string(q);

    stringstream ss(q);
    string drop_word, table_word, table_name, extra;
    ss >> drop_word >> table_word >> table_name >> extra;

    if (to_lower_string(drop_word) != "drop" ||
        to_lower_string(table_word) != "table" ||
        table_name.empty() ||
        !extra.empty()) {
        cout << "Syntax Error: Use DROP TABLE table_name;\n";
        return;
    }

    char tab[MAX_NAME];
    strncpy(tab, table_name.c_str(), MAX_NAME - 1);
    tab[MAX_NAME - 1] = '\0';

    if (search_table(tab) == 0) {
        cout << "ERROR: Table '" << table_name << "' does not exist.\n";
        return;
    }

    string table_path = "./table/" + table_name;

    try {
        if (fs::exists(table_path)) {
            fs::remove_all(table_path);
        }
    } catch (...) {
        cout << "ERROR: Could not remove table directory for '" << table_name << "'.\n";
        return;
    }

    ifstream in("./table/table_list");
    ofstream out("./table/table_list.tmp");

    if (!in.is_open() || !out.is_open()) {
        cout << "ERROR: Could not update table registry.\n";
        return;
    }

    string name;
    while (in >> name) {
        if (name != table_name) {
            out << name << "\n";
        }
    }

    in.close();
    out.close();

    remove("./table/table_list");
    rename("./table/table_list.tmp", "./table/table_list");

    cout << "Success: Table '" << table_name << "' dropped successfully.\n";
}

void tokenize_update(char query[]) {
    string q(query);

    while (!q.empty() && (q.back() == ';' || q.back() == '\n' || q.back() == ' ')) {
        q.pop_back();
    }

    string q_lower = keyword_lower_copy(q);
    size_t set_pos = q_lower.find(" set ");
    size_t where_pos = q_lower.find(" where ");

    if (set_pos == string::npos) {
        cout << "Syntax Error: Missing SET keyword.\n";
        return;
    }
    if (where_pos == string::npos || where_pos <= set_pos) {
        cout << "Syntax Error: Missing WHERE keyword.\n";
        return;
    }

    string before_set = trim_string(q.substr(0, set_pos));
    stringstream ss_tbl(before_set);
    string update_kw, table_name;
    ss_tbl >> update_kw >> table_name;
    if (to_lower_string(update_kw) != "update" || table_name.empty()) {
        cout << "Syntax Error: Use UPDATE table SET col = val WHERE ...\n";
        return;
    }

    string set_clause = trim_string(q.substr(set_pos + 5, where_pos - set_pos - 5));
    size_t eq_pos = set_clause.find('=');
    if (eq_pos == string::npos) {
        cout << "Syntax Error: SET clause must be col = value.\n";
        return;
    }

    string set_col = trim_string(set_clause.substr(0, eq_pos));
    string set_val = remove_quotes(trim_string(set_clause.substr(eq_pos + 1)));
    if (set_col.empty() || set_val.empty()) {
        cout << "Syntax Error: SET column or value is empty.\n";
        return;
    }

    string where_clause = trim_string(q.substr(where_pos + 7));
    size_t where_eq = where_clause.find('=');
    if (where_eq == string::npos) {
        cout << "Syntax Error: WHERE clause must be col = value.\n";
        return;
    }

    string where_col = trim_string(where_clause.substr(0, where_eq));
    string where_val = remove_quotes(trim_string(where_clause.substr(where_eq + 1)));
    if (where_col.empty() || where_val.empty()) {
        cout << "Syntax Error: WHERE column or value is empty.\n";
        return;
    }

    UpdateStatement stmt;
    stmt.table_name = table_name;
    stmt.set_column = set_col;
    stmt.set_value = set_val;
    stmt.where_column = where_col;
    stmt.where_value = where_val;

    execute_update(stmt);
}

void print_query_syntax_help() {
    cout << "\nSupported Query Syntax\n";
    cout << "--------------------------------------------------\n";
    cout << "SHOW TABLES;\n";
    cout << "CREATE TABLE students (id INT, name VARCHAR(50), dept VARCHAR(20));\n";
    cout << "INSERT INTO students VALUES (1, \"Aditya\", \"CSE\");\n";
    cout << "SELECT * FROM students;\n";
    cout << "SELECT name, dept FROM students;\n";
    cout << "SELECT * FROM students WHERE id = 1;\n";
    cout << "UPDATE students SET dept = ECE WHERE id = 1;\n";
    cout << "DROP TABLE students;\n";
    cout << "--------------------------------------------------\n\n";
}

void execute_query_string(string input_query) {
    input_query = trim_string(input_query);

    if (input_query.empty()) {
        cout << "Error: Empty query.\n";
        return;
    }

    stringstream ss(input_query);
    string first_word;
    ss >> first_word;

    string token_temp = to_lower_string(first_word);

    char final_query[1024];
    strncpy(final_query, input_query.c_str(), sizeof(final_query) - 1);
    final_query[sizeof(final_query) - 1] = '\0';

    if (token_temp == "select") {
        tokenize_select(final_query);
    }
    else if (token_temp == "create") {
        tokenize_create(final_query);
    }
    else if (token_temp == "insert") {
        tokenize_insert(final_query);
    }
    else if (token_temp == "show") {
        tokenize_show(final_query);
    }
    else if (token_temp == "drop") {
        tokenize_drop(final_query);
    }
    else if (token_temp == "update") {
        tokenize_update(final_query);
    }
    else {
        cout << "\nError: Wrong syntax or unsupported command.\n";
        print_query_syntax_help();
    }
}
