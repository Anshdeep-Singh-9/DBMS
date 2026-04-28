#include "insert.h"
#include "file_handler.h"
#include "BPtree.h"
#include "buffer_pool_manager.h"
#include "disk_manager.h"
#include "data_page.h"
#include "recovery_manager.h"
#include "tuple_serializer.h"
#include <string>
#include <vector>
#include <iostream>
#include <cstring>
#include <limits>

// Removed local search_table definition as it is now in display.h/display.cpp

/*
 * What:
 * This insert path now uses the BufferPoolManager instead of talking directly
 * to DiskManager for every page access.
 *
 * Why:
 * Insert is one of the first places where page reuse matters. While searching
 * for a page with free space, the engine may touch several pages. A buffer
 * pool lets us cache those pages in RAM and update them through frames.
 *
 * Understanding:
 * Flow of one insert:
 * 1. Build tuple values in memory
 * 2. Serialize tuple to bytes
 * 3. Ask buffer pool for candidate pages
 * 4. If a page has space, modify that page in RAM
 * 5. Write a redo record into the recovery log
 * 6. Mark the frame dirty and unpin it
 * 7. Buffer pool flushes dirty pages back to disk later
 * 8. Store key -> RID in the B+ Tree
 *
 * Concept used:
 * - page-based storage
 * - slotted pages
 * - RID based row addressing
 * - buffer pool caching
 * - write-ahead logging
 * - dirty page tracking
 * - pin / unpin discipline
 *
 * Layman version:
 * - before, insert directly touched the disk page file
 * - now, insert first works on a RAM copy of the page, logs the final page
 *   image for crash recovery, and then lets the buffer pool send it to disk
 */
void insert_command(char tname[], const std::vector<TupleValue>& values, const std::vector<ColumnSchema>& schema) {
    // 1. Prepare the Index
    BPtree index(tname);
    
    // Check if primary key already exists (assuming first column is PK)
    int pk_value = values[0].int_value;
    if (index.search(pk_value).page_id != INVALID_PAGE_ID) {
        std::cout << "Error: Primary Key " << pk_value << " already exists." << std::endl;
        return;
    }

    // 2. Serialize the data into a tuple
    std::vector<char> tuple_data;
    if (!TupleSerializer::serialize(schema, values, tuple_data)) {
        std::cout << "Error: Failed to serialize tuple." << std::endl;
        return;
    }

    // 3. Manage the Data Storage (Page-Based)
    std::string data_path = "table/";
    data_path += tname;
    data_path += "/data.dat";
    
    DiskManager data_disk(data_path, STORAGE_PAGE_SIZE);
    if (!data_disk.open_or_create()) {
        std::cout << "Error: Could not open data file." << std::endl;
        return;
    }

    BufferPoolManager buffer_pool(4, &data_disk);

    // 4. Find a page with enough space
    uint32_t target_page_id = INVALID_PAGE_ID;
    DataPage page;
    bool found_space = false;
    char* target_buffer = NULL;

    // Search existing pages for space
    for (uint32_t i = 0; i < data_disk.page_count(); ++i) {
        char* frame_data = buffer_pool.fetch_page(i);
        if (frame_data == NULL) {
            continue;
        }

        page.load_from_buffer(frame_data, STORAGE_PAGE_SIZE);
        if (page.can_store(tuple_data.size())) {
            target_page_id = i;
            found_space = true;
            target_buffer = frame_data;
            break;
        }

        buffer_pool.unpin_page(i, false);
    }

    
    if (!found_space) {
        target_buffer = buffer_pool.new_page(target_page_id);
        if (target_buffer == NULL) {
            std::cout << "Error: Could not allocate a new page in the buffer pool." << std::endl;
            return;
        }

        page.initialize(target_page_id);
        std::memcpy(target_buffer, page.data(), STORAGE_PAGE_SIZE);
    }

    // 5. Insert tuple into the in-memory page frame
    uint16_t slot_id;
    if (!page.insert_tuple(tuple_data.data(), tuple_data.size(), slot_id)) {
        std::cout << "Error: Failed to insert tuple into page." << std::endl;
        buffer_pool.unpin_page(target_page_id, false);
        return;
    }

    RecoveryTicket recovery_ticket =
        RecoveryManager::log_insert_redo(tname, target_page_id, slot_id, pk_value, page.data());
    if (!recovery_ticket.valid) {
        std::cout << "Error: Failed to write recovery log." << std::endl;
        buffer_pool.unpin_page(target_page_id, false);
        return;
    }

    std::memcpy(target_buffer, page.data(), STORAGE_PAGE_SIZE);
    buffer_pool.unpin_page(target_page_id, true);
    buffer_pool.flush_page(target_page_id);

    // 6. Update the B+ Tree Index with the new RID
    RID rid(target_page_id, slot_id);
    index.insert(pk_value, rid);
    RecoveryManager::mark_insert_applied(recovery_ticket);

    std::cout << "Successfully inserted row into " << tname << " at RID(" 
              << target_page_id << ", " << slot_id << ")" << std::endl;
}

void insert(){
    char tab[MAX_NAME];
    std::cout << "enter table name: ";
    std::cin >> tab;
    
    if(search_table(tab) == 0){
        printf("\nTable \" %s \" don't exist\n", tab);
        return ;
    }

    // Load Table Metadata
    table meta;
    FilePtr fp = open_file_read(tab, "r");
    if (!fp) return;
    fread(&meta, sizeof(table), 1, fp);
    fclose(fp);

    std::cout << "\nTable exists, enter data for " << meta.count << " columns:\n";

    std::vector<ColumnSchema> schema;
    std::vector<TupleValue> values;

    for(int i = 0; i < meta.count; i++){
        std::cout << meta.col[i].col_name << " (" 
                  << (meta.col[i].type == INT ? "INT" : "VARCHAR") << "): ";
        
        ColumnSchema col_schema(meta.col[i].col_name, 
                                meta.col[i].type == INT ? STORAGE_COLUMN_INT : STORAGE_COLUMN_VARCHAR,
                                meta.col[i].size);
        schema.push_back(col_schema);

        if(meta.col[i].type == INT){
            int val;
            if (!(std::cin >> val)) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Error: Invalid INT value for column '"
                          << meta.col[i].col_name << "'. Insert cancelled.\n";
                return;
            }
            values.push_back(TupleValue::FromInt(val));
        } else {
            std::string val;
            std::cin >> std::ws;
            std::getline(std::cin, val);
            values.push_back(TupleValue::FromVarchar(val));
        }
    }

    insert_command(tab, values, schema);
}
