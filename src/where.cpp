#include "where.h"
#include "BPtree.h"
#include "data_page.h"
#include "disk_manager.h"
#include "display.h"
#include "file_handler.h"
#include "tuple_serializer.h"
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

/*
 * What:
 * execute_select_where handles SELECT ... WHERE col = value.
 *
 * Why:
 * A WHERE clause needs two different strategies depending on whether
 * the filter column is the indexed primary key or a regular column.
 *
 * Routing logic:
 *   - WHERE on col[0] (primary key, INT type)
 *       -> B+ Tree point lookup:  O(log n)
 *       -> Returns exactly one RID (page_id, slot_id) and reads that slot
 * directly
 *
 *   - WHERE on any other column, or a VARCHAR primary key
 *       -> Linear scan: O(n)
 *       -> Iterates every page, deserializes every row, compares at the right
 * index
 *
 * Understanding:
 *   The B+ Tree in this project stores  int_key -> RID(page_id, slot_id).
 *   A successful search gives us the exact physical location of the row,
 *   so we read only ONE page instead of scanning all pages.
 *
 * Case sensitivity:
 *   Column names and string values are compared as-is.
 *   "Name" != "name"
 */

static void print_row(const std::vector<TupleValue> &values,
                      const std::vector<int> &col_indices_to_print) {
  for (int idx : col_indices_to_print) {
    if (values[idx].type == STORAGE_COLUMN_INT) {
      std::cout << std::left << std::setw(20) << values[idx].int_value;
    } else {
      std::cout << std::left << std::setw(20) << values[idx].string_value;
    }
  }
  std::cout << "\n";
}

static bool row_matches_where(const std::vector<TupleValue> &values,
                              int where_col_idx, const WhereClause &where) {
  const TupleValue &cell = values[where_col_idx];
  if (cell.type == STORAGE_COLUMN_INT) {

    try {
      int target = std::stoi(where.value);
      return (cell.int_value == target);
    } catch (...) {
      return false;
    }
  } else {

    return (cell.string_value == where.value);
  }
}

static void search_via_bptree(const std::string &tab_name, struct table *meta,
                              const std::vector<ColumnSchema> &schema,
                              const std::vector<int> &col_indices_to_print,
                              const WhereClause &where) {
  std::cout << "\n[Search Strategy: B+ Tree Point Lookup on Primary Key]\n";

  int pk_value;
  try {
    pk_value = std::stoi(where.value);
  } catch (...) {
    std::cout << "Error: WHERE value '" << where.value
              << "' cannot be parsed as INT for primary key column '"
              << where.column << "'.\n";
    return;
  }

  BPtree index(tab_name.c_str());
  RID rid = index.search(pk_value);

  if (rid.page_id == INVALID_PAGE_ID) {
    std::cout << "(No record found with " << where.column << " = " << pk_value
              << ")\n";
    return;
  }

  std::string data_path = "table/" + tab_name + "/data.dat";
  DiskManager data_disk(data_path);
  if (!data_disk.open_or_create()) {
    std::cout << "Error: Could not open data file.\n";
    return;
  }

  char page_buffer[STORAGE_PAGE_SIZE];
  if (!data_disk.read_page(rid.page_id, page_buffer)) {
    std::cout << "Error: Could not read page " << rid.page_id << ".\n";
    return;
  }

  DataPage page;
  page.load_from_buffer(page_buffer, STORAGE_PAGE_SIZE);

  std::vector<char> tuple_data;
  if (!page.read_tuple(rid.slot_id, tuple_data)) {
    std::cout << "Error: Could not read slot " << rid.slot_id << " from page "
              << rid.page_id << ".\n";
    return;
  }

  std::vector<TupleValue> values;
  if (!TupleSerializer::deserialize(schema, tuple_data, values)) {
    std::cout << "Error: Failed to deserialize tuple.\n";
    return;
  }

  print_row(values, col_indices_to_print);
  std::cout << "---------------------------------------------------------------"
               "-----------------\n";
  std::cout << "Total rows: 1\n\n";
}

static void search_via_linear_scan(const std::string &tab_name,
                                   struct table *meta,
                                   const std::vector<ColumnSchema> &schema,
                                   const std::vector<int> &col_indices_to_print,
                                   const WhereClause &where,
                                   int where_col_idx) {
  std::cout << "\n[Search Strategy: Linear Scan]\n";

  std::string data_path = "table/" + tab_name + "/data.dat";
  DiskManager data_disk(data_path);
  if (!data_disk.open_or_create()) {
    std::cout << "Error: Could not open data file.\n";
    return;
  }

  int row_count = 0;

  for (uint32_t i = 0; i < data_disk.page_count(); ++i) {
    char page_buffer[STORAGE_PAGE_SIZE];
    if (!data_disk.read_page(i, page_buffer))
      continue;

    DataPage page;
    page.load_from_buffer(page_buffer, STORAGE_PAGE_SIZE);

    for (uint16_t slot_id = 0; slot_id < page.slot_count(); ++slot_id) {
      std::vector<char> tuple_data;
      if (!page.read_tuple(slot_id, tuple_data))
        continue;

      std::vector<TupleValue> values;
      if (!TupleSerializer::deserialize(schema, tuple_data, values))
        continue;

      if (row_matches_where(values, where_col_idx, where)) {
        print_row(values, col_indices_to_print);
        row_count++;
      }
    }
  }

  std::cout << "---------------------------------------------------------------"
               "-----------------\n";
  if (row_count == 0) {
    std::cout << "(No records found matching WHERE " << where.column << " = "
              << where.value << ")\n";
  }
  std::cout << "Total rows: " << row_count << "\n\n";
}

void execute_select_where(const std::string &tab_name,
                          const std::vector<std::string> &target_cols,
                          const WhereClause &where) {

  char tab[MAX_NAME];
  strncpy(tab, tab_name.c_str(), MAX_NAME - 1);
  tab[MAX_NAME - 1] = '\0';

  if (search_table(tab) == 0) {
    std::cout << "\nTable \"" << tab_name << "\" doesn't exist.\n";
    return;
  }

  struct table *meta = fetch_meta_data(tab_name);
  if (!meta)
    return;

  int where_col_idx = -1;
  for (int i = 0; i < meta->count; i++) {
    if (where.column == meta->col[i].col_name) {
      where_col_idx = i;
      break;
    }
  }
  if (where_col_idx == -1) {
    std::cout << "Error: Column '" << where.column
              << "' does not exist in table '" << tab_name << "'.\n";
    delete meta;
    return;
  }

  bool select_all = (target_cols.size() == 1 && target_cols[0] == "*");
  std::vector<int> col_indices_to_print;

  for (const auto &t_col : target_cols) {
    if (select_all)
      break;
    bool found = false;
    for (int i = 0; i < meta->count; i++) {
      if (t_col == meta->col[i].col_name) {
        col_indices_to_print.push_back(i);
        found = true;
        break;
      }
    }
    if (!found) {
      std::cout << "Error: Column '" << t_col << "' does not exist.\n";
      delete meta;
      return;
    }
  }
  if (select_all) {
    for (int i = 0; i < meta->count; i++)
      col_indices_to_print.push_back(i);
  }

  std::vector<ColumnSchema> schema;
  for (int i = 0; i < meta->count; i++) {
    schema.emplace_back(meta->col[i].col_name,
                        meta->col[i].type == INT ? STORAGE_COLUMN_INT
                                                 : STORAGE_COLUMN_VARCHAR,
                        meta->col[i].size);
  }

  std::cout << "\n-------------------------------------------------------------"
               "-------------------\n";
  for (int idx : col_indices_to_print) {
    std::cout << std::left << std::setw(20) << meta->col[idx].col_name;
  }
  std::cout << "\n-------------------------------------------------------------"
               "-------------------\n";

  bool is_pk_column = (where_col_idx == 0);
  bool pk_is_int = (meta->col[0].type == INT);

  if (is_pk_column && pk_is_int) {
    search_via_bptree(tab_name, meta, schema, col_indices_to_print, where);
  } else {
    search_via_linear_scan(tab_name, meta, schema, col_indices_to_print, where,
                           where_col_idx);
  }

  delete meta;
}
