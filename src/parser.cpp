#include "parser.h"
#include "display.h"
#include<cstring>
#include<iostream>
#include<algorithm>

//tokenize select query for processing
void tokenize_select(char query[]){
    char buffer[1024];
    vector <string> token_vector;
    
    strcpy(buffer, query);
    
    // Uses standard strtok, splitting strictly on spaces, commas, semicolons, and newlines
    char *token = strtok(buffer, " ,;\n"); 
    
    while (token) {
        string token_temp(token);
        if(token_temp != " " && token_temp != "\n" ){
            token_vector.push_back(token_temp);
        }
        token = strtok(NULL, " ,;\n"); 
    }
    process_select(token_vector);
}

//tokenize create query for processing
void tokenize_create(char query[]){
    char buffer[1024];
    vector <string> token_vector;

    strcpy(buffer, query);
    
    char *token = strtok(buffer, " ,;\n");
    while (token) {
        string token_temp(token);
        if(token_temp != " " && token_temp != "\n" ){
          std::transform(token_temp.begin(), token_temp.end(), token_temp.begin(), ::tolower);
            token_vector.push_back(token_temp);
        }
        token = strtok(NULL, " ,;\n");
    }
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

    char buffer[1024];
    strcpy(buffer, query);
    
    char *token = strtok(buffer, " \n");
    if(token){
        string token_temp(token);
        if(token_temp != " " && token_temp != "\n"){
            if(token_temp == "select"){
                tokenize_select(query);
            }else if(token_temp == "create"){
                tokenize_create(query);
            }
        }
    }
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