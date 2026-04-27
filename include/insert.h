#ifndef INSERT_H
#define INSERT_H

#include "declaration.h"
#include "tuple_serializer.h"
#include <vector>

void insert_command(char tname[], const std::vector<TupleValue>& values, const std::vector<ColumnSchema>& schema);

#endif
