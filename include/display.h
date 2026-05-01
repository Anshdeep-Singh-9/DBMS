#ifndef DISPLAY_H
#define DISPLAY_H

#include "declaration.h"
#include "tuple_serializer.h"
#include "query_result.h"
#include <vector>
#include <string>
#include <map>

extern void process_select(std::vector <std::string> &token_vector, QueryResult* res = nullptr);
extern void execute_select(const std::string& tab_name,
                    const std::vector<std::string>& target_cols, QueryResult* res = nullptr);
extern void display_meta_data();
extern void display(); // New function for option 5
extern int search_table(char tab_name[]);
void print_table(const std::vector<ColumnSchema>& schema, const std::vector<std::vector<TupleValue>>& table_data);

#endif
