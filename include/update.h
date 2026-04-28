#ifndef UPDATE_H
#define UPDATE_H

#include "declaration.h"
#include <string>

struct UpdateStatement {
    std::string table_name;
    std::string set_column;
    std::string set_value;
    std::string where_column;
    std::string where_value;
};

extern void execute_update(const UpdateStatement& stmt);

#endif
