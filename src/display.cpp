#include "display.h"
#include "buffer_pool_manager.h"
#include "data_page.h"
#include "disk_manager.h"
#include "file_handler.h"
#include "tuple_serializer.h"
#include "where.h"
#include <iomanip>
#include <iostream>

#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define BLUE    "\033[34m"
#define WHITE   "\033[97m"

void print_table(const std::vector<ColumnSchema>& schema, const std::vector<std::vector<TupleValue>>& table_data) {
    if (schema.empty()) return;

    // Print Header
    std::cout << "\n" << BOLD << BLUE;
    for (const auto& col : schema) {
        std::cout << std::left << std::setw(20) << col.name;
    }
    std::cout << RESET << "\n";

    // Print Separator
    std::cout << BLUE;
    for (size_t i = 0; i < schema.size(); ++i) {
        std::cout << "--------------------";
    }
    std::cout << RESET << "\n";

    // Print Rows
    std::cout << WHITE;
    for (const auto& row : table_data) {
        for (const auto& val : row) {
            if (val.type == STORAGE_COLUMN_INT) {
                std::cout << std::left << std::setw(20) << val.int_value;
            } else {
                std::cout << std::left << std::setw(20) << val.string_value;
            }
        }
        std::cout << "\n";
    }
    std::cout << RESET;

    // Print Bottom Separator
    std::cout << BLUE;
    for (size_t i = 0; i < schema.size(); ++i) {
        std::cout << "--------------------";
    }
    std::cout << RESET << "\n";
    std::cout << BOLD << WHITE << "Total rows: " << table_data.size() << RESET << "\n\n";
}

/*
 * What:
 * The display path now reads table pages through BufferPoolManager instead of
 * directly asking DiskManager for every page.
 *
 * Why:
 * Insert was already moved onto the new page-based storage path. To complete
 * the core read/write story, display should also use the same RAM-side cache
 * layer. That proves the engine can both write rows into pages and fetch them
 * back through a DBMS-style memory path.
 *
 * Understanding:
 * Flow of display:
 * 1. Read table metadata and build the tuple schema
 * 2. Open data.dat using DiskManager
 * 3. Ask BufferPoolManager for each page
 * 4. Load that page into DataPage
 * 5. Read each tuple slot
 * 6. Deserialize bytes into values
 * 7. Print the values
 * 8. Unpin the page because display only reads it
 *
 * Concept used:
 * - page-based storage
 * - buffer pool cache hit/miss
 * - read-only unpin with is_dirty = false
 * - tuple deserialization
 *
 * Layman version:
 * - before, display directly opened pages from disk every time
 * - now, display first checks the RAM cache for a page and only goes to disk
 *   if that page is not already in memory
 */

// Internal helper for search_table
int search_table(char tab_name[]) {
  char str[MAX_NAME + 1];
  strcpy(str, "grep -Fxq ");
  strcat(str, tab_name);
  strcat(str, " ./table/table_list");
  int x = system(str);
  if (x == 0)
    return 1;
  else
    return 0;
}

void display() {
  char tab[MAX_NAME];
  std::cout << "enter table name: ";
  std::cin >> tab;

  if (search_table(tab) == 0) {
    printf("\nTable \" %s \" don't exist\n", tab);
    return;
  }

  // Load Table Metadata
  struct table *meta = fetch_meta_data(tab);
  if (!meta)
    return;

  std::cout << "\nDisplaying contents for table: " << tab << "\n";
  std::cout << "---------------------------------------------------------------"
               "-----------------\n";
  for (int i = 0; i < meta->count; i++) {
    std::cout << std::left << std::setw(20) << meta->col[i].col_name;
  }
  std::cout << "\n-------------------------------------------------------------"
               "-------------------\n";

  // Prepare Schema for Deserialization
  std::vector<ColumnSchema> schema;
  for (int i = 0; i < meta->count; i++) {
    ColumnSchema col_schema(meta->col[i].col_name,
                            meta->col[i].type == INT ? STORAGE_COLUMN_INT
                                                     : STORAGE_COLUMN_VARCHAR,
                            meta->col[i].size);
    schema.push_back(col_schema);
  }

  // Open Data Storage
  std::string data_path = "table/";
  data_path += tab;
  data_path += "/data.dat";

  DiskManager data_disk(data_path);
  if (!data_disk.open_or_create()) {
    std::cout << "Error: Could not open data file." << std::endl;
    delete meta;
    return;
  }

  BufferPoolManager buffer_pool(4, &data_disk);

  int row_count = 0;
  // Iterate through all pages
  for (uint32_t i = 0; i < data_disk.page_count(); ++i) {
    char *frame_data = buffer_pool.fetch_page(i);
    if (frame_data == NULL) {
      continue;
    }

    DataPage page;
    page.load_from_buffer(frame_data, STORAGE_PAGE_SIZE);

    // Iterate through all slots in the page
    for (uint16_t slot_id = 0; slot_id < page.slot_count(); ++slot_id) {
      std::vector<char> tuple_data;
      if (page.read_tuple(slot_id, tuple_data)) {
        std::vector<TupleValue> values;
        if (TupleSerializer::deserialize(schema, tuple_data, values)) {
          for (const auto &val : values) {
            if (val.type == STORAGE_COLUMN_INT) {
              std::cout << std::left << std::setw(20) << val.int_value;
            } else {
              std::cout << std::left << std::setw(20) << val.string_value;
            }
          }
          std::cout << "\n";
          row_count++;
        }
      }
    }

    buffer_pool.unpin_page(i, false);
  }

  if (row_count == 0) {
    std::cout << "(Table is empty)\n";
  }

  std::cout << "---------------------------------------------------------------"
               "-----------------\n";
  std::cout << "Total rows: " << row_count << "\n\n";
  delete meta;
}

void show_tables() {
  char name[MAX_NAME];
  printf("\n\nLIST OF TABLES\n\n");
  printf("---------------------------\n");
  int tabcount = 0;
  FilePtr fp = fopen("./table/table_list", "r");
  if (fp) {
    while (fscanf(fp, "%s", name) != EOF) {
      tabcount++;
      std::cout << name << "\n";
    }
    fclose(fp);
  }
  if (tabcount == 0)
    printf("Database empty!!!\n");
  printf("---------------------------\n");
}

void display_meta_data() {
  std::string name;
  std::cout << "Enter the name of table: ";
  std::cin >> name;

  if (name.empty()) {
    std::cout << "ERROR! No name entered. Exiting!!!\n";
    return;
  }

  struct table *t_ptr = fetch_meta_data(name);
  if (t_ptr == NULL) {
    std::cout << "ERROR! Table not found or failed to read metadata.\n";
    return;
  }
  std::cout << "Table Name: " << t_ptr->name << std::endl << std::endl;
  std::cout << "Max Row Size: " << t_ptr->size << std::endl << std::endl;
  std::cout << "Table Attributes:\n";
  for (int i = 0; i < t_ptr->count; i++) {
    std::cout << "name: " << t_ptr->col[i].col_name << std::endl;
    std::cout << "type: " << ((t_ptr->col[i].type == 1) ? "INT" : "VARCHAR")
              << std::endl;
    std::cout << "size: " << t_ptr->col[i].size << std::endl << std::endl;
  }
  delete t_ptr;
}

void display(char tab[], std::map<std::string, int> &display_col_list);

void execute_select(const std::string &tab_name,
                    const std::vector<std::string> &target_cols) {
  char tab[MAX_NAME];
  strcpy(tab, tab_name.c_str());

  if (search_table(tab) == 0) {
    std::cout << "\nTable \"" << tab << "\" doesn't exist\n";
    return;
  }

  struct table *meta = fetch_meta_data(tab);
  if (!meta)
    return;

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
      std::cout << "Error: Column '" << t_col << "' does not exist in table.\n";
      delete meta;
      return;
    }
  }

  if (select_all) {
    for (int i = 0; i < meta->count; i++)
      col_indices_to_print.push_back(i);
  }

  std::cout << "\n-------------------------------------------------------------"
               "-------------------\n";
  for (int idx : col_indices_to_print) {
    std::cout << std::left << std::setw(20) << meta->col[idx].col_name;
  }
  std::cout << "\n-------------------------------------------------------------"
               "-------------------\n";

  std::vector<ColumnSchema> schema;
  for (int i = 0; i < meta->count; i++) {
    ColumnSchema col_schema(meta->col[i].col_name,
                            meta->col[i].type == INT ? STORAGE_COLUMN_INT
                                                     : STORAGE_COLUMN_VARCHAR,
                            meta->col[i].size);
    schema.push_back(col_schema);
  }

  std::string data_path = "table/" + tab_name + "/data.dat";
  DiskManager data_disk(data_path);
  if (!data_disk.open_or_create()) {
    std::cout << "Error: Could not open data file.\n";
    delete meta;
    return;
  }

  int row_count = 0;
  for (uint32_t i = 0; i < data_disk.page_count(); ++i) {
    char buffer[STORAGE_PAGE_SIZE];
    if (!data_disk.read_page(i, buffer))
      continue;

    DataPage page;
    page.load_from_buffer(buffer, STORAGE_PAGE_SIZE);

    for (uint16_t slot_id = 0; slot_id < page.slot_count(); ++slot_id) {
      std::vector<char> tuple_data;
      if (page.read_tuple(slot_id, tuple_data)) {
        std::vector<TupleValue> values;
        if (TupleSerializer::deserialize(schema, tuple_data, values)) {

          for (int idx : col_indices_to_print) {
            if (values[idx].type == STORAGE_COLUMN_INT) {
              std::cout << std::left << std::setw(20) << values[idx].int_value;
            } else {
              std::cout << std::left << std::setw(20)
                        << values[idx].string_value;
            }
          }
          std::cout << "\n";
          row_count++;
        }
      }
    }
  }

  if (row_count == 0)
    std::cout << "(Table is empty)\n";
  std::cout << "---------------------------------------------------------------"
               "-----------------\n";
  std::cout << "Total rows: " << row_count << "\n\n";
  delete meta;
}

void process_select(std::vector<std::string> &token_vector) {
  if (token_vector.size() < 4 || token_vector[0] != "select") {
    std::cout << "Syntax Error: Invalid SELECT statement.\n";
    return;
  }

  std::vector<std::string> target_cols;
  std::string table_name = "";
  int from_index = -1;

  for (size_t i = 1; i < token_vector.size(); i++) {
    if (token_vector[i] == "from") {
      from_index = i;
      break;
    } else {
      target_cols.push_back(token_vector[i]);
    }
  }
  if (from_index == -1 || (int)token_vector.size() <= from_index + 1) {
    std::cout << "Syntax Error: Missing FROM clause or table name.\n";
    return;
  }

  table_name = token_vector[from_index + 1];

  WhereClause where_clause;
  int where_start = from_index + 2;

  if ((int)token_vector.size() > where_start &&
      token_vector[where_start] == "where") {

    if ((int)token_vector.size() < where_start + 4) {
      std::cout << "Syntax Error: Incomplete WHERE clause.\n"
                << "Usage: WHERE <column> = <value>\n";
      return;
    }

    std::string op = token_vector[where_start + 2];
    if (op != "=") {
      std::cout << "Syntax Error: Only '=' is supported in WHERE clause.\n";
      return;
    }

    where_clause.present = true;
    where_clause.column = token_vector[where_start + 1];
    where_clause.op = op;
    where_clause.value = token_vector[where_start + 3];
  }

  if (where_clause.present) {
    execute_select_where(table_name, target_cols, where_clause);
  } else {
    execute_select(table_name, target_cols);
  }
}