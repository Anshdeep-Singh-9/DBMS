#include "parser.h"
#include "display.h"
#include<cstring>
#include<iostream>
#include<algorithm>

//tokenize select query for processing
//tokenize select query for processing
void tokenize_select(char query[]){
    vector<string> token_vector;
    string current_token = "";
    bool in_quotes = false;

    for(int i = 0; query[i] != '\0'; i++){
        char c = query[i];

        // If we hit a quote, toggle our "inside quotes" state and add the quote to the token
        if(c == '"'){
            in_quotes = !in_quotes;
            current_token += c; 
        }
        // If we are NOT in quotes, treat spaces, commas, semicolons, and newlines as split points
        else if(!in_quotes && (c == ' ' || c == ',' || c == ';' || c == '\n')){
            if(!current_token.empty()){
                token_vector.push_back(current_token);
                current_token = "";
            }
        }
        // Otherwise, just keep building the current word
        else {
            if(c != '\n') {
                current_token += c;
            }
        }
    }
    
    // Catch the very last token if the string ended
    if(!current_token.empty()){
        token_vector.push_back(current_token);
    }

    process_select(token_vector);
}

//tokenize create query for processing
void tokenize_create(char query[]){
    // CREATE TABLE [IF NOT EXISTS] <tablename> (<column1> <datatype>, <column2> <datatype>, PRIMARY KEY(<column1>));

    /*
    * implementation1
    CREATE TABLE <tablename> (<column1> <datatype>, <column2> <datatype>);
    */
    char buffer[1024];
    vector <string> token_vector;

    strcpy(buffer, query);
    
    // FIX: Added \n here as well so CREATE queries don't suffer from the same newline bug
    char *token = strtok(buffer, " ,;\n");
    while (token) {
        string token_temp(token);
        if(token_temp != " " && token_temp != "\n" ){
          std::transform(token_temp.begin(), token_temp.end(), token_temp.begin(), ::tolower);
            token_vector.push_back(token_temp);
        }
        token = strtok(NULL, " ,;\n");
    }

    // for(int i=0;i<token_vector.size();i++){
    //     cout<<token_vector[i]<<endl;
    // }
    cout<<endl;
}

void get_query(){
    char *query;
    query = (char*) malloc (sizeof(char)*50);
    printf("enter query to test\n");
    fflush(stdin);
    fflush(stdout);
    fgets(query,sizeof(char)*MAX_NAME,stdin);
    fgets(query,sizeof(char)*MAX_NAME,stdin);

    //
    char buffer[1024];
    strcpy(buffer, query);
    
    // FIX: Added \n here to ensure the first command word is caught cleanly
    char *token = strtok(buffer, " \n");
    if(token){
        string token_temp(token);
        if(token_temp != " " && token_temp != "\n"){
            //cout<<"token:: "<<token<<endl;
            if(token_temp == "select"){
                tokenize_select(query);
            }else if(token_temp == "create"){
                tokenize_create(query);
            }
        }
    }


    //printf("\nquery:: %s\n",query);
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