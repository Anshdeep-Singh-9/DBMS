#ifndef WHERE_H
#define WHERE_H

#include "declaration.h"
#include "tuple_serializer.h"
#include "query_result.h"
#include <string>
#include <vector>

/*
 * WhereClause captures the parsed predicate from a SQL WHERE clause.
 *
 * present  -> false means no WHERE clause was given (run full scan)
 * column   -> the column name to filter on (case-sensitive)
 * op       -> only "=" is supported
 * value    -> the raw string representation of the search value
 *
 * Routing logic:
 *   - If column == col[0] (primary key) AND col[0].type == INT
 *       -> B+ Tree point lookup  (O(log n))
 *   - Otherwise
 *       -> Linear scan over all pages  (O(n))
 */
struct WhereClause {
  bool present;
  std::string column;
  std::string op;
  std::string value;

  WhereClause() : present(false), column(""), op(""), value("") {}
};

/*
 * execute_select_where
 *
 * Runs a SELECT query that has a WHERE predicate attached.
 * Internally decides between B+ Tree lookup and linear scan.
 *
 * Parameters:
 *   tab_name    -> table to query
 *   target_cols -> columns to print ("*" means all)
 *   where_clause -> parsed WHERE predicate
 *   res          -> optional QueryResult to store results
 */
extern void execute_select_where(const std::string &tab_name,
                                 const std::vector<std::string> &target_cols,
                                 const WhereClause &where_clause,
                                 QueryResult* res = nullptr);

#endif
