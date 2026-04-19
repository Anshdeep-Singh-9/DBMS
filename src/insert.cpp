#include "insert.h"
#include "file_handler.h"
#include "BPtree.h"
#include "disk_manager.h"
#include "data_page.h"
#include "tuple_serializer.h"
#include "aes.h"
#include <string>
#include <vector>
#include <iostream>

// Removed local search_table definition as it is now in display.h/display.cpp

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

    // 4. Find a page with enough space
    uint32_t target_page_id = INVALID_PAGE_ID;
    DataPage page;
    bool found_space = false;

    // Search existing pages for space
    for (uint32_t i = 0; i < data_disk.page_count(); ++i) {
        char buffer[STORAGE_PAGE_SIZE];
        data_disk.read_page(i, buffer);
        page.load_from_buffer(buffer, STORAGE_PAGE_SIZE);
        if (page.can_store(tuple_data.size())) {
            target_page_id = i;
            found_space = true;
            break;
        }
    }

    // If no space, allocate a new page
    if (!found_space) {
        target_page_id = data_disk.allocate_page();
        page.initialize(target_page_id);
    }

    // 5. Insert tuple into page and write back to disk
    uint16_t slot_id;
    if (!page.insert_tuple(tuple_data.data(), tuple_data.size(), slot_id)) {
        std::cout << "Error: Failed to insert tuple into page." << std::endl;
        return;
    }
    data_disk.write_page(target_page_id, page.data());

    // 6. Update the B+ Tree Index with the new RID
    RID rid(target_page_id, slot_id);
    index.insert(pk_value, rid);

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
            std::cin >> val;
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
