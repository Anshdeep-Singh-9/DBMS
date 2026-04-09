#include "where.h"
#include "BPtree.h"

void select_particular_query(std::string table_name, std::string col_to_search, std::string col_value,std::map<std::string, int> &display_col_list){
    
    // --- TRACK QUOTES ---
    bool has_quotes = false;
    if (col_value.length() >= 2 && col_value.front() == '"' && col_value.back() == '"') {
        col_value = col_value.substr(1, col_value.length() - 2);
        has_quotes = true;
    }

    //process the particular query
    char *tab = (char*)malloc(sizeof(char)*MAX_NAME);
    strcpy(tab, table_name.c_str());
    int check = search_table(tab);
    if(check == 1){
        /*
        access the meta data of table
        */
        FILE *fp = open_file(tab, const_cast<char*>("r"));
        if(display_col_list.find("all_columns_set") != display_col_list.end()){
            table *temp;
            temp = (table*)malloc(sizeof(table));
            if(fp){
                display_col_list.clear();
                fread(temp,1,sizeof(table),fp);
                for(int i = 0; i < temp->count; i++){
                    std::string temp_str(temp->col[i].col_name);
                    display_col_list.insert(make_pair(temp_str,0));
                }
            }else{
                printf("\ninternal server error\nexiting...\n\n");
                return;
            }
            std::map <std::string, int> :: iterator it;
            for(it = display_col_list.begin(); it != display_col_list.end(); it++){
            }
        }
        if(display_col_list.find("all_columns_set") == display_col_list.end()){
            table *input;
            FILE *fpall = open_file(tab, const_cast<char*>("r"));
            input = (table*)malloc(sizeof(table));
			if(fpall){
				fread(input, sizeof(table), 1, fpall);
				for(int k = 0; k < input->count; k++){
					std::string temp_str(input->col[k].col_name);
					std::map <std::string, int>::iterator it = display_col_list.find(temp_str);
					if(it != display_col_list.end()){
						it->second = 1;
					}
				}
				std::map <std::string, int> :: iterator it = display_col_list.begin();
				for(it = display_col_list.begin(); it != display_col_list.end(); it++){
					if(it->second == 0){
						printf("\nerror\n");
						std::cout << it->first << " is not a valid column for table " << input->name << "\n";
						return;
					}
				}
			}else{
				printf("\ninternal server error\nexiting...\n");
				return;
			}
		}
        FILE *fpall2 = open_file(tab, const_cast<char*>("r"));
        if(fpall2){
            int pri_int;
            int c;
            char d[MAX_NAME];
            char pri_char[MAX_NAME];
            BPtree mytree(tab);
            int ret=0;
            table *temp2 = (table*)malloc(sizeof(table));
            fread(temp2, sizeof(table), 1, fpall2);
            if(strcmp(temp2->col[0].col_name,col_to_search.c_str()) == 0){
                if(temp2->col[0].type == INT){
                    pri_int = atoi( col_value.c_str());
                    ret = mytree.get_record(pri_int);
                    if (ret == BPTREE_SEARCH_NOT_FOUND){
                       printf("\nkey %d don't exist!!!\n", pri_int);
                   }else{
                       FILE *fpz;
                       char *str1;
                       printf("\n------------------------------------\n");
                       str1 = (char*)malloc(sizeof(char)*MAX_PATH);
                       sprintf(str1,"table/%s/file%d.dat",tab,ret);
                       fpz = fopen(str1,"r");
                       for(int j = 0; j < temp2->count; j++){
                           std::string temp_str(temp2->col[j].col_name);
                               if(temp2->col[j].type == INT){
                                   fread(&c,1,sizeof(int), fpz);
                                   if(display_col_list.find(temp_str) != display_col_list.end())
                                      std::cout << c << setw(20);
                               }
                               else if(temp2->col[j].type == VARCHAR){
                                   fread(d,1,sizeof(char)*MAX_NAME,fpz);
                                   if(display_col_list.find(temp_str) != display_col_list.end())
                                      std::cout << d << setw(20);
                               }
                       }
                       printf("\n------------------------------------\n");
                       fclose(fpz);
                       free(str1);
                   }
               }else if(temp2->col[0].type == VARCHAR){
                   if (!has_quotes) {
                       printf("\nSyntax error: String literals must be enclosed in double quotes (\"\").\n\n");
                       return;
                   }
                   strcpy(pri_char,col_value.c_str());
                   void *arr[MAX_NAME];
                   arr[0] = (char*)malloc(sizeof(char)*MAX_NAME);
                   arr[0] = pri_char;
                   ret = mytree.get_record(*(int*)arr[0]);
                   if (ret == BPTREE_SEARCH_NOT_FOUND){
                       printf("\nkey %s don't exist !!!\n", pri_char);
                   }
                   else{
                       FILE *fpz;
                       char *str1;
                       str1 = (char*)malloc(sizeof(char)*MAX_PATH);
                       sprintf(str1,"table/%s/file%d.dat",tab,ret);
                       fpz = fopen(str1,"r");
                       printf("\n------------------------------------\n");
                       for(int j = 0; j < temp2->count; j++){
                           std::string temp_str(temp2->col[j].col_name);
                               if(temp2->col[j].type == INT){
                                   fread(&c, 1, sizeof(int), fpz);
                                   if(display_col_list.find(temp_str) != display_col_list.end())
                                   std::cout << c << setw(20);
                               }
                               else if(temp2->col[j].type == VARCHAR){
                                   fread(d, 1, sizeof(char)*MAX_NAME, fpz);
                                   if(display_col_list.find(temp_str) != display_col_list.end())
                                   std::cout << d << setw(20);
                               }
                       }
                       printf("\n-------------------------------------\n");
                       fclose(fpz);
                       free(str1);
                   }
               }
            }else{
                int col_number = 1;
                int col_type = 0;
                int flag = 0;
                FILE *fpbf = open_file(tab, const_cast<char*>("r"));
                table * tempbf = (table*)malloc(sizeof(table));
                fread(tempbf,sizeof(table),1,fpbf);
                for(int i = 0; i < tempbf->count ;i++){
                    if(strcmp(tempbf->col[i].col_name,col_to_search.c_str()) == 0){
                        col_number = i + 1;
                        col_type = tempbf->col[i].type;
                        flag = 1;
                        break;
                    }
                }
                if(flag == 0){
                    printf("\ncolumn doesn't exist\nexiting...\n\n");
                    return ;
                }else{
                    if (col_type == VARCHAR && !has_quotes) {
                        printf("\nSyntax error: String literals must be enclosed in double quotes (\"\").\n\n");
                        return;
                    }
                    flag = 0;
                    int c;
                    char d[MAX_NAME];
                    for(int i=0;i<tempbf->rec_count;i++){
                            FILE *fpr;
                            char *str;
                            str=(char*)malloc(sizeof(char)*MAX_PATH);
                            sprintf(str,"table/%s/file%d.dat",tab,i);
                            fpr=fopen(str,"r");
                            for(int j = 0; j < col_number; j++){
                                    if(tempbf->col[j].type == INT){
                                        fread(&c,1,sizeof(int),fpr);
                                    }else if(tempbf->col[j].type == VARCHAR){
                                        fread(d,1,sizeof(char)*MAX_NAME,fpr);
                                    }
                            }
                            if(col_type == INT){
                                int col_int_value = atoi( col_value.c_str());
                                if(col_int_value == c){
                                    int c1;
                                    char d1[MAX_NAME];
                                    fclose(fpr);
                                    fpr = fopen(str,"r");
                                    printf("\n-------------------------------------\n");
                                    for(int j = 0; j < tempbf->count; j++){
                                        std::string temp_str(tempbf->col[j].col_name);
                                            if(tempbf->col[j].type == INT){
                                                fread(&c1,1,sizeof(int),fpr);
                                                if(display_col_list.find(temp_str) != display_col_list.end())
                                                std::cout << c1 << setw(20);
                                            }
                                            else if(tempbf->col[j].type == VARCHAR){
                                                fread(d1,1,sizeof(char)*MAX_NAME,fpr);
                                                if(display_col_list.find(temp_str) != display_col_list.end())
                                                std::cout << d1 << setw(20);
                                            }
                                    }
                                    printf("\n-------------------------------------\n");
                                    flag = 1;
                                }
                            }else if(col_type == VARCHAR){
                                if(strcmp(d,col_value.c_str()) == 0){
                                    int c1;
                                    char d1[MAX_NAME];
                                    fclose(fpr);
                                    fpr=fopen(str,"r");
                                    printf("\n-------------------------------------\n");
                                    for(int j = 0; j < tempbf->count; j++){
                                        std::string temp_str(tempbf->col[j].col_name);
                                            if(tempbf->col[j].type == INT){
                                                fread(&c1, 1, sizeof(int), fpr);
                                                if(display_col_list.find(temp_str) != display_col_list.end())
                                                std::cout << c1 << setw(20);
                                            }
                                            else if(tempbf->col[j].type == VARCHAR){
                                                fread(d1,1,sizeof(char)*MAX_NAME,fpr);
                                                if(display_col_list.find(temp_str) != display_col_list.end())
                                                std::cout << d1 << setw(20);
                                            }
                                    }
                                    printf("\n-------------------------------------\n");
                                    flag = 1;
                                }
                            }else{
                                printf("\ninternal server error\nexiting...\n\n");
                                return;
                            }
                            std::cout << "\n\n";
                            free(str);
                            fclose(fpr);
                            if(flag == 1){
                                break;
                            }
                    }
                }
            }
        }
    }
}