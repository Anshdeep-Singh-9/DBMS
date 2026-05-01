#include "auth.h"
#include "sha256.h"
#include "file_handler.h"
#include "disk_manager.h"
#include "buffer_pool_manager.h"
#include "data_page.h"
#include "tuple_serializer.h"
#include "BPtree.h"
#include "vacuum.h"
#include <iostream>
#include <random>
#include <iomanip>
#include <cstring>

std::unordered_map<std::string, std::string> AuthManager::sessions;

bool AuthManager::init() {
    struct table* meta = fetch_system_meta_data("auth");
    if (!meta) {
        create_auth_table();
    } else {
        delete meta;
    }
    return true;
}

void AuthManager::create_auth_table() {
    table* temp = new table();
    strcpy(temp->name, "auth");
    temp->count = 3;
    
    // Column 0: id (INT) - Primary Key for B+ Tree
    strcpy(temp->col[0].col_name, "id");
    temp->col[0].type = INT;
    temp->col[0].size = 4;

    // Column 1: username (VARCHAR 50)
    strcpy(temp->col[1].col_name, "username");
    temp->col[1].type = VARCHAR;
    temp->col[1].size = 50;

    // Column 2: password_hash (VARCHAR 64)
    strcpy(temp->col[2].col_name, "password_hash");
    temp->col[2].type = VARCHAR;
    temp->col[2].size = 64;

    // Calculate record size
    int size = 0;
    temp->prefix[0] = 0;
    temp->prefix[1] = sizeof(int);
    size += sizeof(int);
    temp->prefix[2] = (MAX_VARCHAR + 1) + temp->prefix[1];
    size += (MAX_VARCHAR + 1);
    temp->prefix[3] = (64 + 1) + temp->prefix[2];
    size += (64 + 1);
    
    temp->size = size;
    temp->data_size = 0;
    temp->rec_count = 0;

    store_system_meta_data(temp);

    // Create empty data file and B+ Tree
    std::string data_path = "./system/auth/data.dat";
    DiskManager dm(data_path);
    dm.open_or_create();
    
    // Note: BPtree implementation seems to assume "./table/TNAME/" path
    // For simplicity, we'll store system indexes in the same place but prefixed with __sys_
    BPtree index((char*)"__sys_auth");
    
    delete temp;
}

bool AuthManager::register_user(const std::string& username, const std::string& password) {
    if (user_exists(username)) return false;

    struct table* meta = fetch_system_meta_data("auth");
    if (!meta) return false;

    std::vector<ColumnSchema> schema;
    schema.push_back(ColumnSchema("id", STORAGE_COLUMN_INT, 4));
    schema.push_back(ColumnSchema("username", STORAGE_COLUMN_VARCHAR, 50));
    schema.push_back(ColumnSchema("password_hash", STORAGE_COLUMN_VARCHAR, 64));

    // Simple auto-increment ID based on rec_count
    int id = meta->rec_count + 1;
    std::string hash = SHA256::hash(password);

    std::vector<TupleValue> values;
    values.push_back(TupleValue::FromInt(id));
    values.push_back(TupleValue::FromVarchar(username));
    values.push_back(TupleValue::FromVarchar(hash));

    std::vector<char> tuple_data;
    TupleSerializer::serialize(schema, values, tuple_data);

    std::string data_path = "./system/auth/data.dat";
    DiskManager data_disk(data_path);
    data_disk.open_or_create();
    BufferPoolManager buffer_pool(4, &data_disk);

    uint32_t target_page_id = INVALID_PAGE_ID;
    DataPage page;
    char* target_buffer = NULL;

    if (data_disk.page_count() == 0) {
        target_buffer = buffer_pool.new_page(target_page_id);
        page.initialize(target_page_id);
    } else {
        target_page_id = data_disk.page_count() - 1;
        target_buffer = buffer_pool.fetch_page(target_page_id);
        page.load_from_buffer(target_buffer, STORAGE_PAGE_SIZE);
        if (!page.can_store(tuple_data.size())) {
            if (compact_page_buffer(target_buffer)) {
                page.load_from_buffer(target_buffer, STORAGE_PAGE_SIZE);
            }
        }
        if (!page.can_store(tuple_data.size())) {
            buffer_pool.unpin_page(target_page_id, false);
            target_buffer = buffer_pool.new_page(target_page_id);
            page.initialize(target_page_id);
        }
    }

    uint16_t slot_id;
    page.insert_tuple(tuple_data.data(), tuple_data.size(), slot_id);
    std::memcpy(target_buffer, page.data(), STORAGE_PAGE_SIZE);
    buffer_pool.unpin_page(target_page_id, true);
    buffer_pool.flush_page(target_page_id);

    BPtree index((char*)"__sys_auth");
    index.insert(id, RID(target_page_id, slot_id));

    meta->rec_count++;
    store_system_meta_data(meta);
    delete meta;

    return true;
}

bool AuthManager::authenticate(const std::string& username, const std::string& password) {
    struct table* meta = fetch_system_meta_data("auth");
    if (!meta) return false;

    std::vector<ColumnSchema> schema;
    schema.push_back(ColumnSchema("id", STORAGE_COLUMN_INT, 4));
    schema.push_back(ColumnSchema("username", STORAGE_COLUMN_VARCHAR, 50));
    schema.push_back(ColumnSchema("password_hash", STORAGE_COLUMN_VARCHAR, 64));

    std::string data_path = "./system/auth/data.dat";
    DiskManager data_disk(data_path);
    if (!data_disk.open_or_create()) {
        delete meta;
        return false;
    }
    BufferPoolManager buffer_pool(4, &data_disk);

    std::string input_hash = SHA256::hash(password);

    for (uint32_t i = 0; i < data_disk.page_count(); ++i) {
        char* frame_data = buffer_pool.fetch_page(i);
        DataPage page;
        page.load_from_buffer(frame_data, STORAGE_PAGE_SIZE);

        for (uint16_t slot_id = 0; slot_id < page.slot_count(); ++slot_id) {
            std::vector<char> tuple_data;
            if (page.read_tuple(slot_id, tuple_data)) {
                std::vector<TupleValue> values;
                TupleSerializer::deserialize(schema, tuple_data, values);
                if (values[1].string_value == username && values[2].string_value == input_hash) {
                    buffer_pool.unpin_page(i, false);
                    delete meta;
                    return true;
                }
            }
        }
        buffer_pool.unpin_page(i, false);
    }

    delete meta;
    return false;
}

bool AuthManager::user_exists(const std::string& username) {
    struct table* meta = fetch_system_meta_data("auth");
    if (!meta) return false;

    std::vector<ColumnSchema> schema;
    schema.push_back(ColumnSchema("id", STORAGE_COLUMN_INT, 4));
    schema.push_back(ColumnSchema("username", STORAGE_COLUMN_VARCHAR, 50));
    schema.push_back(ColumnSchema("password_hash", STORAGE_COLUMN_VARCHAR, 64));

    std::string data_path = "./system/auth/data.dat";
    DiskManager data_disk(data_path);
    if (!data_disk.open_or_create()) {
        delete meta;
        return false;
    }
    BufferPoolManager buffer_pool(4, &data_disk);

    for (uint32_t i = 0; i < data_disk.page_count(); ++i) {
        char* frame_data = buffer_pool.fetch_page(i);
        DataPage page;
        page.load_from_buffer(frame_data, STORAGE_PAGE_SIZE);

        for (uint16_t slot_id = 0; slot_id < page.slot_count(); ++slot_id) {
            std::vector<char> tuple_data;
            if (page.read_tuple(slot_id, tuple_data)) {
                std::vector<TupleValue> values;
                TupleSerializer::deserialize(schema, tuple_data, values);
                if (values[1].string_value == username) {
                    buffer_pool.unpin_page(i, false);
                    delete meta;
                    return true;
                }
            }
        }
        buffer_pool.unpin_page(i, false);
    }

    delete meta;
    return false;
}

bool AuthManager::has_any_user() {
    struct table* meta = fetch_system_meta_data("auth");
    if (!meta) return false;
    bool exists = meta->rec_count > 0;
    delete meta;
    return exists;
}

std::string AuthManager::create_session(const std::string& username) {
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(alphabet) - 2);

    std::string token;
    for (int i = 0; i < 32; ++i) {
        token += alphabet[dis(gen)];
    }

    sessions[token] = username;
    return token;
}

bool AuthManager::validate_session(const std::string& token) {
    return sessions.find(token) != sessions.end();
}

void AuthManager::end_session(const std::string& token) {
    sessions.erase(token);
}

std::string AuthManager::get_user_from_session(const std::string& token) {
    if (validate_session(token)) return sessions[token];
    return "";
}
