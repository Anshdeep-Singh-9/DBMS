#include "crow_all.h"
#include "declaration.h"
#include "file_handler.h"
#include "buffer_pool_manager.h"
#include "data_page.h"
#include "disk_manager.h"
#include "tuple_serializer.h"
#include "create.h"
#include "insert.h"
#include "recovery_manager.h"
#include <vector>
#include <string>

// Function to convert table data to JSON
crow::json::wvalue table_to_json(const std::string& table_name) {
    struct table* meta = fetch_meta_data(table_name);
    if (!meta) {
        return crow::json::wvalue({{"error", "Table not found"}});
    }

    // Prepare Schema
    std::vector<ColumnSchema> schema;
    for (int i = 0; i < meta->count; i++) {
        ColumnSchema col_schema(meta->col[i].col_name,
                                meta->col[i].type == INT ? STORAGE_COLUMN_INT
                                                         : STORAGE_COLUMN_VARCHAR,
                                meta->col[i].size);
        schema.push_back(col_schema);
    }

    // Open Data Storage
    std::string data_path = "table/" + table_name + "/data.dat";
    DiskManager data_disk(data_path);
    if (!data_disk.open_or_create()) {
        delete meta;
        return crow::json::wvalue({{"error", "Could not open data file"}});
    }

    BufferPoolManager buffer_pool(4, &data_disk);
    std::vector<crow::json::wvalue> rows;

    // Iterate through all pages
    for (uint32_t i = 0; i < data_disk.page_count(); ++i) {
        char* frame_data = buffer_pool.fetch_page(i);
        if (frame_data == NULL) continue;

        DataPage page;
        page.load_from_buffer(frame_data, STORAGE_PAGE_SIZE);

        for (uint16_t slot_id = 0; slot_id < page.slot_count(); ++slot_id) {
            std::vector<char> tuple_data;
            if (page.read_tuple(slot_id, tuple_data)) {
                std::vector<TupleValue> values;
                if (TupleSerializer::deserialize(schema, tuple_data, values)) {
                    crow::json::wvalue row;
                    for (size_t j = 0; j < values.size(); ++j) {
                        if (values[j].type == STORAGE_COLUMN_INT) {
                            row[schema[j].name] = values[j].int_value;
                        } else {
                            row[schema[j].name] = values[j].string_value;
                        }
                    }
                    rows.push_back(std::move(row));
                }
            }
        }
        buffer_pool.unpin_page(i, false);
    }

    delete meta;
    return crow::json::wvalue(rows);
}

int main() {
    system_check();
    RecoveryManager::recover_all_tables();
    crow::SimpleApp app;

    // Route to get all entries in a table
    CROW_ROUTE(app, "/table/<string>")
    ([](std::string table_name) {
        auto result = table_to_json(table_name);
        return result;
    });

    // Route to get metadata of a table
    CROW_ROUTE(app, "/meta/<string>")
    ([](std::string table_name) {
        struct table* meta = fetch_meta_data(table_name);
        if (!meta) {
            return crow::response(404, "Table not found");
        }

        crow::json::wvalue result;
        result["table_name"] = meta->name;
        result["column_count"] = meta->count;
        result["record_size"] = meta->size;

        std::vector<crow::json::wvalue> columns;
        for (int i = 0; i < meta->count; i++) {
            crow::json::wvalue col;
            col["name"] = meta->col[i].col_name;
            col["type"] = (meta->col[i].type == INT ? "INT" : "VARCHAR");
            col["size"] = meta->col[i].size;
            columns.push_back(std::move(col));
        }
        result["columns"] = std::move(columns);

        delete meta;
        return crow::response(result);
    });

    // Route to list all tables
    CROW_ROUTE(app, "/tables")
    ([]() {
        std::vector<std::string> tables;
        std::ifstream fp("./table/table_list");
        std::string name;
        if (fp.is_open()) {
            while (fp >> name) {
                tables.push_back(name);
            }
            fp.close();
        }
        
        crow::json::wvalue result;
        result["tables"] = tables;
        return crow::response(result);
    });

    // Route to create a table
    CROW_ROUTE(app, "/create").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) {
        auto x = crow::json::load(req.body);
        if (!x || !x.has("table_name") || !x.has("columns")) {
            return crow::response(400, "Invalid JSON");
        }

        std::string table_name = x["table_name"].s();
        std::vector<std::pair<std::string, std::string>> columns;
        
        for (auto& col : x["columns"]) {
            if (!col.has("name") || !col.has("type")) {
                return crow::response(400, "Invalid column definition");
            }
            columns.push_back({col["name"].s(), col["type"].s()});
        }

        execute_create_query(table_name, columns);
        return crow::response(201, "Table created (check logs for success/failure)");
    });

    // Route to insert data into a table
    CROW_ROUTE(app, "/insert/<string>").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, std::string table_name) {
        auto x = crow::json::load(req.body);
        if (!x) {
            return crow::response(400, "Invalid JSON");
        }

        struct table* meta = fetch_meta_data(table_name);
        if (!meta) {
            return crow::response(404, "Table not found");
        }

        std::vector<ColumnSchema> schema;
        std::vector<TupleValue> values;

        for (int i = 0; i < meta->count; i++) {
            std::string col_name = meta->col[i].col_name;
            ColumnSchema col_schema(col_name,
                                    meta->col[i].type == INT ? STORAGE_COLUMN_INT
                                                             : STORAGE_COLUMN_VARCHAR,
                                    meta->col[i].size);
            schema.push_back(col_schema);

            if (!x.has(col_name)) {
                delete meta;
                return crow::response(400, "Missing column: " + col_name);
            }

            if (meta->col[i].type == INT) {
                values.push_back(TupleValue::FromInt(x[col_name].i()));
            } else {
                values.push_back(TupleValue::FromVarchar(x[col_name].s()));
            }
        }

        char tname[MAX_NAME];
        strcpy(tname, table_name.c_str());
        insert_command(tname, values, schema);

        delete meta;
        return crow::response(200, "Insertion triggered (check logs for success/failure)");
    });

    // Route for bulk insertion into a table
    CROW_ROUTE(app, "/bulk_insert/<string>").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, std::string table_name) {
        auto x = crow::json::load(req.body);
        if (!x || x.size() == 0) {
            return crow::response(400, "Invalid JSON or empty array");
        }

        struct table* meta = fetch_meta_data(table_name);
        if (!meta) {
            return crow::response(404, "Table not found");
        }

        std::vector<ColumnSchema> schema;
        for (int i = 0; i < meta->count; i++) {
            schema.push_back(ColumnSchema(meta->col[i].col_name,
                                         meta->col[i].type == INT ? STORAGE_COLUMN_INT
                                                                  : STORAGE_COLUMN_VARCHAR,
                                         meta->col[i].size));
        }

        std::vector<std::vector<TupleValue>> all_values;
        for (auto& row : x) {
            std::vector<TupleValue> values;
            bool valid_row = true;
            for (int i = 0; i < meta->count; i++) {
                std::string col_name = meta->col[i].col_name;
                if (!row.has(col_name)) {
                    valid_row = false;
                    break;
                }
                if (meta->col[i].type == INT) {
                    values.push_back(TupleValue::FromInt(row[col_name].i()));
                } else {
                    values.push_back(TupleValue::FromVarchar(row[col_name].s()));
                }
            }
            if (valid_row) {
                all_values.push_back(values);
            }
        }

        char tname[MAX_NAME];
        strcpy(tname, table_name.c_str());
        bulk_insert_command(tname, all_values, schema);

        delete meta;
        return crow::response(200, "Bulk insertion triggered (check logs for success/failure)");
    });

    // Health check route
    CROW_ROUTE(app, "/health")
    ([]() {
        return "MiniDB API is running!";
    });

    app.port(18080).multithreaded().run();
}
