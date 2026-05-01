#include "delete.h"
#include "BPtree.h"
#include "data_page.h"
#include "disk_manager.h"
#include "display.h"
#include "file_handler.h"
#include "storage_types.h"
#include "tuple_serializer.h"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#define RESET  "\033[0m"
#define BOLD   "\033[1m"
#define GREEN  "\033[32m"
#define RED    "\033[31m"
#define YELLOW "\033[33m"
#define CYAN   "\033[36m"

static int delete_via_bptree(const std::string& tab_name,
                             const std::vector<ColumnSchema>& schema,
                             const DeleteStatement& stmt) {
    (void)schema;
    std::cout << CYAN << "\n[Delete Strategy: B+ Tree Lookup on Primary Key]" << RESET << "\n";

    int pk_value;
    try {
        size_t used = 0;
        pk_value = std::stoi(stmt.where_value, &used);
        if (used != stmt.where_value.size()) throw std::invalid_argument("");
    } catch (...) {
        std::cout << RED << "Error: WHERE value '" << stmt.where_value
                  << "' is not a valid INT for primary key column '"
                  << stmt.where_column << "'." << RESET << "\n";
        return 0;
    }

    BPtree index(tab_name.c_str());
    RID rid = index.search(pk_value);

    if (rid.page_id == INVALID_PAGE_ID) {
        std::cout << YELLOW << "No row found with "
                  << stmt.where_column << " = " << pk_value << "." << RESET << "\n";
        return 0;
    }

    std::string data_path = "table/" + tab_name + "/data.dat";
    DiskManager disk(data_path);
    if (!disk.open_or_create()) {
        std::cout << RED << "Error: Cannot open data file." << RESET << "\n";
        return 0;
    }

    char buf[STORAGE_PAGE_SIZE];
    if (!disk.read_page(rid.page_id, buf)) {
        std::cout << RED << "Error: Cannot read page " << rid.page_id << "." << RESET << "\n";
        return 0;
    }

    SlotEntry* slots = reinterpret_cast<SlotEntry*>(buf + sizeof(PageHeader));
    slots[rid.slot_id].length = 0;

    if (!disk.write_page(rid.page_id, buf)) {
        std::cout << RED << "Error: Cannot write updated page." << RESET << "\n";
        return 0;
    }

    bool page_empty = true;
    PageHeader* header = reinterpret_cast<PageHeader*>(buf);
    for (uint16_t i = 0; i < header->slot_count; ++i) {
        if (slots[i].length > 0) {
            page_empty = false;
            break;
        }
    }
    if (page_empty) {
        disk.deallocate_page(rid.page_id);
    }

    index.remove_key(pk_value);

    std::cout << GREEN << BOLD << "Deleted 1 row where "
              << stmt.where_column << " = " << pk_value << "." << RESET << "\n";
    return 1;
}

static int delete_via_linear_scan(const std::string& tab_name,
                                  const std::vector<ColumnSchema>& schema,
                                  const DeleteStatement& stmt,
                                  int where_col_idx) {
    std::cout << CYAN << "\n[Delete Strategy: Linear Scan]" << RESET << "\n";

    std::string data_path = "table/" + tab_name + "/data.dat";
    DiskManager disk(data_path);
    if (!disk.open_or_create()) {
        std::cout << RED << "Error: Cannot open data file." << RESET << "\n";
        return 0;
    }

    BPtree index(tab_name.c_str());
    int deleted_count = 0;

    for (uint32_t page_id = 0; page_id < disk.page_count(); ++page_id) {
        char buf[STORAGE_PAGE_SIZE];
        if (!disk.read_page(page_id, buf)) continue;

        DataPage dp;
        dp.load_from_buffer(buf, STORAGE_PAGE_SIZE);

        bool page_dirty = false;

        for (uint16_t slot_id = 0; slot_id < dp.slot_count(); ++slot_id) {
            std::vector<char> old_bytes;
            if (!dp.read_tuple(slot_id, old_bytes)) continue;

            std::vector<TupleValue> values;
            if (!TupleSerializer::deserialize(schema, old_bytes, values)) continue;

            const TupleValue& cell = values[where_col_idx];
            bool matches = false;
            if (cell.type == STORAGE_COLUMN_INT) {
                try { matches = (cell.int_value == std::stoi(stmt.where_value)); }
                catch (...) { matches = false; }
            } else {
                matches = (cell.string_value == stmt.where_value);
            }

            if (!matches) continue;

            SlotEntry* slots = reinterpret_cast<SlotEntry*>(buf + sizeof(PageHeader));
            slots[slot_id].length = 0;
            page_dirty = true;

            int pk_value = values[0].int_value;
            index.remove_key(pk_value);
            deleted_count++;
        }

        if (page_dirty) {
            disk.write_page(page_id, buf);

            bool page_empty = true;
            PageHeader* header = reinterpret_cast<PageHeader*>(buf);
            SlotEntry* slots = reinterpret_cast<SlotEntry*>(buf + sizeof(PageHeader));
            for (uint16_t i = 0; i < header->slot_count; ++i) {
                if (slots[i].length > 0) {
                    page_empty = false;
                    break;
                }
            }
            if (page_empty) {
                disk.deallocate_page(page_id);
            }
        }
    }

    if (deleted_count == 0) {
        std::cout << YELLOW << "No rows matched WHERE "
                  << stmt.where_column << " = " << stmt.where_value << "." << RESET << "\n";
    } else {
        std::cout << GREEN << BOLD << "Deleted " << deleted_count << " row(s) where "
                  << stmt.where_column << " = " << stmt.where_value << "." << RESET << "\n";
    }

    return deleted_count;
}

void execute_delete(const DeleteStatement& stmt) {
    char tab[MAX_NAME];
    strncpy(tab, stmt.table_name.c_str(), MAX_NAME - 1);
    tab[MAX_NAME - 1] = '\0';

    if (search_table(tab) == 0) {
        std::cout << RED << "Error: Table '" << stmt.table_name
                  << "' does not exist." << RESET << "\n";
        return;
    }

    table* meta = fetch_meta_data(stmt.table_name);
    if (meta == NULL) {
        std::cout << RED << "Error: Could not load metadata for '"
                  << stmt.table_name << "'." << RESET << "\n";
        return;
    }

    int where_col_idx = -1;
    for (int i = 0; i < meta->count; i++) {
        if (stmt.where_column == meta->col[i].col_name) {
            where_col_idx = i;
            break;
        }
    }

    if (where_col_idx == -1) {
        std::cout << RED << "Error: Column '" << stmt.where_column
                  << "' does not exist in table '" << stmt.table_name << "'." << RESET << "\n";
        delete meta;
        return;
    }

    std::vector<ColumnSchema> schema;
    for (int i = 0; i < meta->count; i++) {
        schema.emplace_back(
            meta->col[i].col_name,
            meta->col[i].type == INT ? STORAGE_COLUMN_INT : STORAGE_COLUMN_VARCHAR,
            static_cast<uint16_t>(meta->col[i].size)
        );
    }

    bool is_pk_where = (where_col_idx == 0 && meta->col[0].type == INT);
    if (is_pk_where) {
        delete_via_bptree(stmt.table_name, schema, stmt);
    } else {
        delete_via_linear_scan(stmt.table_name, schema, stmt, where_col_idx);
    }

    delete meta;
}
