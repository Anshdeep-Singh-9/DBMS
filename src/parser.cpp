#include "parser.h"
#include "display.h"

#include <cstring>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <cctype>

using namespace std;

string to_lower_query(string q) {
    bool in_quotes = false;

    for (int i = 0; i < (int)q.size(); i++) {
        if (q[i] == '"' || q[i] == '\'') {
            in_quotes = !in_quotes;
        }

        if (!in_quotes) {
            q[i] = tolower(q[i]);
        }
    }

    return q;
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

extern void execute_create_query(string table_name, vector<pair<string, string>> cols);

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