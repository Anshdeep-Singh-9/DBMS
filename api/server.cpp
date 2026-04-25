#include "crow_all.h"
#include "declaration.h"
#include "file_handler.h"
#include "buffer_pool_manager.h"
#include "data_page.h"
#include "disk_manager.h"
#include "tuple_serializer.h"
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
    crow::SimpleApp app;

    // Route to get all entries in a table
    CROW_ROUTE(app, "/table/<string>")
    ([](std::string table_name) {
        auto result = table_to_json(table_name);
        return result;
    });

    // Health check route
    CROW_ROUTE(app, "/health")
    ([]() {
        return "MiniDB API is running!";
    });

    app.port(18080).multithreaded().run();
}
