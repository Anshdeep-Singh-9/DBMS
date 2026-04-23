#include "create.h"
#include "file_handler.h"
#include <sys/stat.h>

int record_size(table *temp){
    int size = 0;
    temp->prefix[0] = 0;
    for(int i = 0; i < temp->count; i++){
        switch(temp->col[i].type){
        case INT:
            temp->prefix[i+1] = sizeof(int) + temp->prefix[i];
            size += sizeof(int);
            break;
        case VARCHAR:
            temp->prefix[i+1] = sizeof(char)*(MAX_VARCHAR +1) + temp->prefix[i];
            size += (MAX_VARCHAR + 1);
            break;
        }
    }
    return size;
}


table * create_table(char name[], int count){
    std::unordered_set <string> create_col_set;
    table *temp = new table();
    temp->fp = NULL;
    temp->blockbuf = NULL;
    strcpy(temp->name, name);
    temp->count = count;
    temp->rec_count = 0;
    temp->data_size = 0;
    cout << "\nEnter col name, type(1.int 2.varchar), and max size\n";
    for(int i = 0; i < count; i++){
        std::string t_name, type, size;
        std::cin >> t_name >> type >> size;
        if(create_col_set.count(t_name) == 0){
            strcpy(temp->col[i].col_name, t_name.c_str());
            create_col_set.insert(t_name);
        } else return NULL;

        temp->col[i].type = type[0] - 48;
        temp->col[i].size = std::stoi(size);
    }
    return temp;
}



void create(){
    char *name = (char*)malloc(sizeof(char)*MAX_NAME);
    std::cout << "\nEnter table name: \n";
    std::cin >> name;

    if(search_table(name) == 1){
        std::cout << "\nERROR! already exists\n";
        free(name);
        return;
    }

    struct stat st = {0};
    if (stat("./table", &st) == -1) mkdir("./table", 0775);

    FilePtr fp = fopen("./table/table_list", "a+");
    if(fp == NULL){
        free(name);
        return;
    }

    std::cout << "\nenter no. of columns: \n";
    int count;
    std::cin >> count;
    table *temp = create_table(name, count);

    
    if(temp != NULL){
        temp->size = record_size(temp);
        store_meta_data(temp);
        fseek(fp, 0, SEEK_END);
        fprintf(fp, "%s\n", name);
        fclose(fp);
        free(temp);
    } else fclose(fp);
    free(name);
}