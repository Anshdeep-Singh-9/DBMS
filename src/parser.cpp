#include "parser.h"
#include "display.h"
#include<cstring>
#include<iostream>
#include<algorithm>

void tokenize_select(char query[]){
    vector<string> token_vector;
    string current_token = "";
    bool in_quotes = false;
    for(int i = 0; query[i] != '\0'; i++) {
        char c = query[i];
        if(c == '"'){
            in_quotes = !in_quotes;
            current_token += c; 
        }
        else if(!in_quotes && (c == ' '|| c == ',' || c ==';' || c == '\n')){
            if(!current_token.empty()){
                token_vector.push_back(current_token);
                current_token = "";
            }
        } 
        else {
            if(c != '\n') {
                current_token += c;
            }
        }
    }
    
    if(!current_token.empty()){
        token_vector.push_back(current_token);
    }

    process_select(token_vector);
}

// Add this declaration at the top of parser.cpp (below the includes)
extern void execute_create_query(std::string table_name, std::vector<std::pair<std::string, std::string>> cols);

// The new SQL Parser for CREATE TABLE
void tokenize_create(char query[]){
    string q(query);
    
    // 1. Find the bounds of the column definitions (inside parentheses)
    size_t start = q.find('(');
    size_t end = q.rfind(')');
    
    if(start == string::npos || end == string::npos) {
        cout << "Syntax Error: Invalid CREATE TABLE format. Missing parentheses.\n";
        return;
    }

    // 2. Extract Table Name
    // Get everything before '(', e.g., "CREATE TABLE students "
    string before_paren = q.substr(0, start);
    stringstream ss(before_paren);
    string word, table_name;
    
    // Read word by word; the last word before the '(' is our table name
    while(ss >> word) {
        table_name = word; 
    }

    // 3. Extract Column Definitions
    // Get everything inside the parentheses: "id INT, name VARCHAR"
    string cols_str = q.substr(start + 1, end - start - 1);
    vector<pair<string, string>> columns;
    
    stringstream col_stream(cols_str);
    string col_def;
    
    // Split the string by commas
    while(getline(col_stream, col_def, ',')) {
        stringstream def_stream(col_def);
        string col_name, col_type;
        
        // Read the name and type, ignoring any extra spaces
        if(def_stream >> col_name >> col_type) {
            columns.push_back({col_name, col_type});
        }
    }
    
    // 4. Pass the parsed data to the storage engine
    execute_create_query(table_name, columns);
}
void get_query(){
    char *query;
    query = (char*) malloc (sizeof(char)*1024);
    printf("enter query to test\n");
    fflush(stdin);
    fflush(stdout);
    fgets(query, 1024, stdin);
    fgets(query, 1024, stdin);

    char buffer[1024];
    strcpy(buffer, query);
    
    char *token = strtok(buffer, " \n");
    if(token){
        string token_temp(token);
        
        // Convert to lowercase so it works even if the user types "SELECT"
        std::transform(token_temp.begin(), token_temp.end(), token_temp.begin(), ::tolower);
        
        if(token_temp == "select"){
            tokenize_select(query);
        }else if(token_temp == "create"){
            tokenize_create(query);
        } else {
            // NEW: Print error for wrong syntax. 
            // After this prints, the function ends and main.cpp safely asks for the next menu option.
            cout << "\nError: Wrong syntax or unsupported command.\n";
        }
    }
    free(query);
}

void parse_create(){
  string s;
  cout<<"enter create query\n";
  cin.ignore();
  getline (cin, s);
  int openpos = s.find("(");
  int closepos = s.find(")");
  string token = s.substr(0, openpos);
  string tbetween = s.substr(openpos+1, s.length()-openpos-2);
  cout<<token<<endl;
  cout<<tbetween<<endl;
}