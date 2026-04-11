#include "insert.h"
#include "file_handler.h"
#include "BPtree.h"
#include "aes.h"
#include <string>

int search_table(char tab_name[]){
    char str[MAX_NAME+1];
    strcpy(str,"grep -Fxq ");
    strcat(str,tab_name);
    strcat(str," ./table/table_list");
    int x = system(str);
    if(x == 0) return 1;
    else return 0;
}

void insert_command(char tname[], void *data[], int total){
    table *temp;
    int ret;
    BPtree obj(tname);
    FilePtr fp = open_file_read(tname, const_cast<char*>("r"));
    temp = (table*)malloc(sizeof(table));
    fread(temp, sizeof(table), 1, fp);

    ret = obj.insert_record(*((int *)data[0]), temp->rec_count);
    if(ret == 2){
        std::cout << "\nkey already exists\n exiting...\n";
        return ;
    }

    fp = open_file(tname, const_cast<char*>("w+"));
    int file_num = temp->rec_count;
    temp->rec_count = temp->rec_count + 1;
    temp->data_size = total;
    fwrite(temp, sizeof(table), 1, fp);
    fclose(fp);

    char *str = (char *)malloc(sizeof(char)*MAX_PATH);
    sprintf(str, "table/%s/file%d.dat", tname, file_num);
    FilePtr fpr = fopen(str, "w+");
    int x;
    char y[MAX_NAME];
    for(int j = 0; j < temp->count; j++){
        if(temp->col[j].type == INT){
             x = *(int *)data[j];
            fwrite(&x, sizeof(int), 1, fpr);
        }
        else if(temp->col[j].type == VARCHAR){
            strcpy(y, (char *)data[j]);
            fwrite(y, sizeof(char)*MAX_NAME, 1, fpr);
        }
    }
    fclose(fpr);
    free(str);
    free(temp);
}

void insert(){
    char *tab = (char*)malloc(sizeof(char)*MAX_PATH+1);
    std::cout << "enter table name: ";
    std::cin >> tab;
    if(search_table(tab) == 0){
        printf("\nTable \" %s \" don't exist\n", tab);
        free(tab);
        return ;
    }

    std::cout << "\nTable exists, enter data\n\n";
    table inp1;
    FilePtr fp = open_file_read(tab, "r");
    fread(&inp1, sizeof(table), 1, fp);
    fclose(fp);

    int count = inp1.count;
    void * data[MAX_ATTR];
    int total = 0;

    for(int i = 0; i < count; i++){
        if(inp1.col[i].type == INT){
            data[i] = malloc(sizeof(int));
            total += sizeof(int);
            std::string inp_int;
            std::cin >> inp_int;
            *((int*)data[i]) = std::stoi(inp_int);
        } else if(inp1.col[i].type == VARCHAR){
            data[i] = malloc(sizeof(char) * (MAX_NAME + 1));
            std::string input_str;
            
            // Fix: Clear whitespace and use getline for spaces
            std::cin >> std::ws; 
            std::getline(std::cin, input_str);

            if (input_str.length() >= 2 && input_str.front() == '"' && input_str.back() == '"') {
                input_str = input_str.substr(1, input_str.length() - 2);
            }

            strncpy((char*)data[i], input_str.c_str(), MAX_NAME);
            total += sizeof(char) * (MAX_NAME + 1);
        }
    }
    insert_command(tab, data, total);
    free(tab);
}