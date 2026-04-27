#ifndef CREATE_H
#define CREATE_H

#include "declaration.h"
#include <string>
#include <vector>
#include <utility>

void execute_create_query(std::string table_name, std::vector<std::pair<std::string, std::string>> cols);

#endif
