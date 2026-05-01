#ifndef DELETE_H
#define DELETE_H

#include "declaration.h"
#include <string>

/*
 * DeleteStatement holds everything parsed from:
 *   DELETE FROM table WHERE col = val
 *
 * Routing (same philosophy as SELECT WHERE / UPDATE WHERE):
 *   WHERE on INT primary key  → B+ Tree lookup   O(log n)
 *   WHERE on any other column → Linear scan       O(n)
 *
 * After marking a slot dead (slot.length = 0), the corresponding
 * B+ Tree entry is also removed so future inserts with the same
 * primary key work correctly.
 */
struct DeleteStatement {
    std::string table_name;
    std::string where_column;
    std::string where_value;
};

extern void execute_delete(const DeleteStatement& stmt);

#endif
