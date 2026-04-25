#include "parser.h"
#include "display.h"
#include "file_handler.h"
#include "insert.h"
#include "tuple_serializer.h"

#include <cstring>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <vector>
#include <cstdlib>

using namespace std;

extern void execute_create_query(string table_name, vector<pair<string, string>> cols);
extern void insert_command(char tname[], const vector<TupleValue>& values, const vector<ColumnSchema>& schema);

string trim_string(string s) {
    while (!s.empty() && isspace((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && isspace((unsigned char)s.back())) s.pop_back();
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

string to_lower_query(string q) {
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

void tokenize_select(char query[]) {
    vector<string> token_vector;
    string current_token = "";
    bool in_quotes = false;

    for (int i = 0; query[i] != '\0'; i++) {
        char c = query[i];

        if (c == '"') {
            in_quotes = !in_quotes;
            current_token += c;
        }
        else if (!in_quotes && (c == ' ' || c == ',' || c == ';' || c == '\n')) {
            if (!current_token.empty()) {
                token_vector.push_back(current_token);
                current_token = "";
            }
        }
        else {
            if (c != '\n') {
                current_token += c;
            }
        }
    }

    if (!current_token.empty()) {
        token_vector.push_back(current_token);
    }

    process_select(token_vector);
}

void tokenize_create(char query[]) {
    string q(query);

    size_t start = q.find('(');
    size_t end = q.rfind(')');

    if (start == string::npos || end == string::npos) {
        cout << "Syntax Error: Invalid CREATE TABLE format. Missing parentheses.\n";
        return;
    }

    string before_paren = q.substr(0, start);
    stringstream ss(before_paren);
    string word, table_name;

    while (ss >> word) {
        table_name = word;
    }

    string cols_str = q.substr(start + 1, end - start - 1);
    vector<pair<string, string>> columns;

    stringstream col_stream(cols_str);
    string col_def;

    while (getline(col_stream, col_def, ',')) {
        stringstream def_stream(col_def);
        string col_name, col_type;

        if (def_stream >> col_name >> col_type) {
            columns.push_back({col_name, col_type});
        }
    }

    execute_create_query(table_name, columns);
}

void tokenize_insert(char query[]) {
    string q(query);

    size_t values_pos = q.find(" values ");
    if (values_pos == string::npos) {
        cout << "Syntax Error: Invalid INSERT statement. Missing VALUES keyword.\n";
        return;
    }

    string before_values = trim_string(q.substr(0, values_pos));
    stringstream ss(before_values);

    string insert_word, into_word, table_name;
    ss >> insert_word >> into_word >> table_name;

    if (insert_word != "insert" || into_word != "into" || table_name.empty()) {
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

void get_query() {
    char *query;
    query = (char*) malloc(sizeof(char) * 1024);

    printf("Enter Query :\n");

    fflush(stdin);
    fflush(stdout);

    fgets(query, 1024, stdin);
    fgets(query, 1024, stdin);

    string lowered_query = to_lower_query(string(query));

    char buffer[1024];
    strcpy(buffer, lowered_query.c_str());

    char *token = strtok(buffer, " \n");

    if (token) {
        string token_temp(token);

        if (token_temp == "select") {
            char final_query[1024];
            strcpy(final_query, lowered_query.c_str());
            tokenize_select(final_query);
        }
        else if (token_temp == "create") {
            char final_query[1024];
            strcpy(final_query, lowered_query.c_str());
            tokenize_create(final_query);
        }
        else if (token_temp == "insert") {
            char final_query[1024];
            strcpy(final_query, lowered_query.c_str());
            tokenize_insert(final_query);
        }
        else {
            cout << "\nError: Wrong syntax or unsupported command.\n";
        }
    }

    free(query);
}

void parse_create() {
    string s;

    cout << "enter create query\n";

    cin.ignore();
    getline(cin, s);

    s = to_lower_query(s);

    int openpos = s.find("(");
    int closepos = s.find(")");

    string token = s.substr(0, openpos);
    string tbetween = s.substr(openpos + 1, s.length() - openpos - 2);

    cout << token << endl;
    cout << tbetween << endl;
}