#include "crow_all.h"
#include "declaration.h"
#include "file_handler.h"
#include "buffer_pool_manager.h"
#include "data_page.h"
#include "disk_manager.h"
#include "tuple_serializer.h"
#include "BPtree.h"
#include "create.h"
#include "insert.h"
#include "recovery_manager.h"
#include "auth.h"
#include "parser.h"
#include <vector>
#include <string>
#include <cctype>

// ---------------------------------------------------------------------------
// Minimal SELECT query parser
// Handles: SELECT */cols FROM table [WHERE col = val]
// ---------------------------------------------------------------------------
struct ParsedSelect {
    bool valid = false;
    std::string error;
    std::string table;
    std::vector<std::string> columns;
    bool has_where = false;
    std::string where_col, where_val;
};

static std::string tok_lower(std::string s) {
    for (char& c : s) c = static_cast<char>(tolower((unsigned char)c));
    return s;
}

static ParsedSelect parse_select_query(const std::string& query) {
    ParsedSelect r;
    std::vector<std::string> tokens;
    std::string cur;
    bool in_dq = false, in_sq = false;
    for (char c : query) {
        if (c == '"' && !in_sq) { in_dq = !in_dq; continue; }
        if (c == '\'' && !in_dq) { in_sq = !in_sq; continue; }
        if (!in_dq && !in_sq && (c == ' ' || c == ',' || c == ';' || c == '\t' || c == '\n')) {
            if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
        } else { cur += c; }
    }
    if (!cur.empty()) tokens.push_back(cur);

    if (tokens.empty() || tok_lower(tokens[0]) != "select") {
        r.error = "Not a SELECT query"; return r;
    }
    int from_pos = -1;
    for (int i = 1; i < (int)tokens.size(); i++) {
        if (tok_lower(tokens[i]) == "from") { from_pos = i; break; }
    }
    if (from_pos == -1 || from_pos + 1 >= (int)tokens.size()) {
        r.error = "Missing FROM or table name"; return r;
    }
    for (int i = 1; i < from_pos; i++) r.columns.push_back(tokens[i]);
    if (r.columns.empty()) { r.error = "No columns specified"; return r; }
    r.table = tokens[from_pos + 1];

    int where_pos = -1;
    for (int i = from_pos + 2; i < (int)tokens.size(); i++) {
        if (tok_lower(tokens[i]) == "where") { where_pos = i; break; }
    }
    if (where_pos != -1) {
        if (where_pos + 3 > (int)tokens.size() - 1) {
            r.error = "Incomplete WHERE clause"; return r;
        }
        r.has_where = true;
        r.where_col = tokens[where_pos + 1];
        for (int i = where_pos + 3; i < (int)tokens.size(); i++) {
            if (!r.where_val.empty()) r.where_val += " ";
            r.where_val += tokens[i];
        }
    }
    r.valid = true;
    return r;
}

// ---------------------------------------------------------------------------
// Execute a parsed SELECT and return JSON { type, strategy, columns, rows, row_count }
// ---------------------------------------------------------------------------
static crow::json::wvalue handle_select_api(const std::string& query) {
    crow::json::wvalue result;
    ParsedSelect ps = parse_select_query(query);
    if (!ps.valid) {
        result["type"] = "error"; result["message"] = ps.error; return result;
    }

    struct table* meta = fetch_meta_data(ps.table);
    if (!meta) {
        result["type"] = "error";
        result["message"] = "Table '" + ps.table + "' does not exist";
        return result;
    }

    std::vector<ColumnSchema> full_schema;
    for (int i = 0; i < meta->count; i++) {
        full_schema.emplace_back(
            meta->col[i].col_name,
            meta->col[i].type == INT ? STORAGE_COLUMN_INT : STORAGE_COLUMN_VARCHAR,
            meta->col[i].size);
    }

    bool select_all = (ps.columns.size() == 1 && ps.columns[0] == "*");
    std::vector<int> col_indices;
    if (select_all) {
        for (int i = 0; i < meta->count; i++) col_indices.push_back(i);
    } else {
        for (const auto& cname : ps.columns) {
            bool found = false;
            for (int i = 0; i < meta->count; i++) {
                if (cname == meta->col[i].col_name) { col_indices.push_back(i); found = true; break; }
            }
            if (!found) {
                result["type"] = "error";
                result["message"] = "Column '" + cname + "' does not exist";
                delete meta; return result;
            }
        }
    }

    std::vector<ColumnSchema> out_schema;
    for (int idx : col_indices) out_schema.push_back(full_schema[idx]);

    int where_col_idx = -1;
    std::string strategy = "Full Table Scan";
    if (ps.has_where) {
        for (int i = 0; i < meta->count; i++) {
            if (ps.where_col == meta->col[i].col_name) { where_col_idx = i; break; }
        }
        if (where_col_idx == -1) {
            result["type"] = "error";
            result["message"] = "WHERE column '" + ps.where_col + "' does not exist";
            delete meta; return result;
        }
        bool pk_where = (where_col_idx == 0 && meta->col[0].type == INT);
        strategy = pk_where ? "B+ Tree Point Lookup  O(log n)" : "Linear Scan  O(n)";
    }

    std::string data_path = "table/" + ps.table + "/data.dat";
    std::vector<std::vector<TupleValue>> result_rows;

    if (ps.has_where && where_col_idx == 0 && meta->col[0].type == INT) {
        // --- B+ Tree path ---
        int pk_val;
        try { pk_val = std::stoi(ps.where_val); }
        catch (...) {
            result["type"] = "error";
            result["message"] = "WHERE value '" + ps.where_val + "' is not a valid INT";
            delete meta; return result;
        }
        BPtree index(ps.table.c_str());
        RID rid = index.search(pk_val);
        if (rid.page_id != INVALID_PAGE_ID) {
            DiskManager disk(data_path);
            if (disk.open_or_create()) {
                char buf[STORAGE_PAGE_SIZE];
                if (disk.read_page(rid.page_id, buf)) {
                    DataPage dp; dp.load_from_buffer(buf, STORAGE_PAGE_SIZE);
                    std::vector<char> td;
                    if (dp.read_tuple(rid.slot_id, td)) {
                        std::vector<TupleValue> vals;
                        if (TupleSerializer::deserialize(full_schema, td, vals)) {
                            std::vector<TupleValue> proj;
                            for (int idx : col_indices) proj.push_back(vals[idx]);
                            result_rows.push_back(proj);
                        }
                    }
                }
            }
        }
    } else {
        // --- Full scan / Linear scan with optional WHERE ---
        DiskManager disk(data_path);
        if (disk.open_or_create()) {
            BufferPoolManager bpm(4, &disk);
            for (uint32_t pg = 0; pg < disk.page_count(); ++pg) {
                char* frame = bpm.fetch_page(pg);
                if (!frame) continue;
                DataPage dp; dp.load_from_buffer(frame, STORAGE_PAGE_SIZE);
                for (uint16_t slot = 0; slot < dp.slot_count(); ++slot) {
                    std::vector<char> td;
                    if (!dp.read_tuple(slot, td)) continue;
                    std::vector<TupleValue> vals;
                    if (!TupleSerializer::deserialize(full_schema, td, vals)) continue;
                    if (ps.has_where) {
                        const TupleValue& cell = vals[where_col_idx];
                        bool match = false;
                        if (cell.type == STORAGE_COLUMN_INT) {
                            try { match = (cell.int_value == std::stoi(ps.where_val)); } catch (...) {}
                        } else { match = (cell.string_value == ps.where_val); }
                        if (!match) continue;
                    }
                    std::vector<TupleValue> proj;
                    for (int idx : col_indices) proj.push_back(vals[idx]);
                    result_rows.push_back(proj);
                }
                bpm.unpin_page(pg, false);
            }
        }
    }

    result["type"]      = "select";
    result["strategy"]  = strategy;
    result["row_count"] = (int)result_rows.size();

    std::vector<crow::json::wvalue> cols_json;
    for (const auto& col : out_schema) {
        crow::json::wvalue c;
        c["name"] = col.name;
        c["type"] = (col.type == STORAGE_COLUMN_INT ? "INT" : "VARCHAR");
        c["size"] = col.max_length;
        cols_json.push_back(std::move(c));
    }
    result["columns"] = std::move(cols_json);

    std::vector<crow::json::wvalue> rows_json;
    for (const auto& row : result_rows) {
        crow::json::wvalue r;
        for (size_t i = 0; i < row.size() && i < out_schema.size(); i++) {
            if (row[i].type == STORAGE_COLUMN_INT) r[out_schema[i].name] = row[i].int_value;
            else                                   r[out_schema[i].name] = row[i].string_value;
        }
        rows_json.push_back(std::move(r));
    }
    result["rows"] = std::move(rows_json);

    delete meta;
    return result;
}

// ---------------------------------------------------------------------------
// Function to convert full table data to JSON (used by GET /table/<name>)
// ---------------------------------------------------------------------------
crow::json::wvalue table_to_json(const std::string& table_name) {
    struct table* meta = fetch_meta_data(table_name);
    if (!meta) {
        return crow::json::wvalue({{"error", "Table not found"}});
    }

    std::vector<ColumnSchema> schema;
    for (int i = 0; i < meta->count; i++) {
        ColumnSchema col_schema(meta->col[i].col_name,
                                meta->col[i].type == INT ? STORAGE_COLUMN_INT
                                                         : STORAGE_COLUMN_VARCHAR,
                                meta->col[i].size);
        schema.push_back(col_schema);
    }

    std::string data_path = "table/" + table_name + "/data.dat";
    DiskManager data_disk(data_path);
    if (!data_disk.open_or_create()) {
        delete meta;
        return crow::json::wvalue({{"error", "Could not open data file"}});
    }

    BufferPoolManager buffer_pool(4, &data_disk);
    std::vector<crow::json::wvalue> rows;

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
                        if (values[j].type == STORAGE_COLUMN_INT)
                            row[schema[j].name] = values[j].int_value;
                        else
                            row[schema[j].name] = values[j].string_value;
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
    AuthManager::init();
    system_check();
    crow::SimpleApp app;

        auto is_authenticated = [](const crow::request& req) {
            auto token = req.get_header_value("X-Session-Token");
            return AuthManager::validate_session(token);
        };

        CROW_ROUTE(app, "/login").methods(crow::HTTPMethod::POST)
        ([](const crow::request& req) {
            auto x = crow::json::load(req.body);
            if (!x || !x.has("username") || !x.has("password")) {
                return crow::response(400, "Invalid JSON: username and password required");
            }
            std::string username = x["username"].s();
            std::string password = x["password"].s();
            if (AuthManager::authenticate(username, password)) {
                std::string token = AuthManager::create_session(username);
                crow::json::wvalue res;
                res["token"] = token;
                res["status"] = "success";
                return crow::response(res);
            } else {
                return crow::response(401, "Invalid credentials");
            }
        });

        CROW_ROUTE(app, "/register").methods(crow::HTTPMethod::POST)
        ([](const crow::request& req) {
            auto x = crow::json::load(req.body);
            if (!x || !x.has("username") || !x.has("password")) {
                return crow::response(400, "Invalid JSON: username and password required");
            }
            std::string username = x["username"].s();
            std::string password = x["password"].s();
            if (AuthManager::user_exists(username)) {
                return crow::response(400, "User already exists");
            }
            if (AuthManager::register_user(username, password)) {
                return crow::response(201, "User registered successfully");
            } else {
                return crow::response(500, "Failed to register user");
            }
        });

        CROW_ROUTE(app, "/logout").methods(crow::HTTPMethod::POST)
        ([&is_authenticated](const crow::request& req) {
            auto token = req.get_header_value("X-Session-Token");
            if (AuthManager::validate_session(token)) {
                AuthManager::end_session(token);
                return crow::response(200, "Logged out successfully");
            }
            return crow::response(401, "Not logged in");
        });

        CROW_ROUTE(app, "/table/<string>")
        ([&is_authenticated](const crow::request& req, std::string table_name) {
            if (!is_authenticated(req)) return crow::response(401, "Authentication required");
            auto result = table_to_json(table_name);
            return crow::response(result);
        });

        CROW_ROUTE(app, "/meta/<string>")
        ([&is_authenticated](const crow::request& req, std::string table_name) {
            if (!is_authenticated(req)) return crow::response(401, "Authentication required");
            struct table* meta = fetch_meta_data(table_name);
            if (!meta) return crow::response(404, "Table not found");

            crow::json::wvalue result;
            result["table_name"]   = meta->name;
            result["column_count"] = meta->count;
            result["record_size"]  = meta->size;

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

    CROW_ROUTE(app, "/tables")
    ([&is_authenticated](const crow::request& req) {
        if (!is_authenticated(req)) return crow::response(401, "Authentication required");
        std::vector<std::string> tables;
        std::ifstream fp("./table/table_list");
        std::string name;
        if (fp.is_open()) {
            while (fp >> name) tables.push_back(name);
            fp.close();
        }
        crow::json::wvalue result;
        result["tables"] = tables;
        return crow::response(result);
    });

    CROW_ROUTE(app, "/create").methods(crow::HTTPMethod::POST)
    ([&is_authenticated](const crow::request& req) {
        if (!is_authenticated(req)) return crow::response(401, "Authentication required");
        auto x = crow::json::load(req.body);
        if (!x || !x.has("table_name") || !x.has("columns")) {
            return crow::response(400, "Invalid JSON");
        }
        std::string table_name = x["table_name"].s();
        std::vector<std::pair<std::string, std::string>> columns;
        for (auto& col : x["columns"]) {
            if (!col.has("name") || !col.has("type")) return crow::response(400, "Invalid column definition");
            columns.push_back({col["name"].s(), col["type"].s()});
        }
        execute_create_query(table_name, columns);
        return crow::response(201, "Table created (check logs for success/failure)");
    });

    CROW_ROUTE(app, "/insert/<string>").methods(crow::HTTPMethod::POST)
    ([&is_authenticated](const crow::request& req, std::string table_name) {
        if (!is_authenticated(req)) return crow::response(401, "Authentication required");
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400, "Invalid JSON");

        struct table* meta = fetch_meta_data(table_name);
        if (!meta) return crow::response(404, "Table not found");

        std::vector<ColumnSchema> schema;
        std::vector<TupleValue> values;
        for (int i = 0; i < meta->count; i++) {
            std::string col_name = meta->col[i].col_name;
            schema.push_back(ColumnSchema(col_name,
                                          meta->col[i].type == INT ? STORAGE_COLUMN_INT : STORAGE_COLUMN_VARCHAR,
                                          meta->col[i].size));
            if (!x.has(col_name)) { delete meta; return crow::response(400, "Missing column: " + col_name); }
            if (meta->col[i].type == INT) values.push_back(TupleValue::FromInt(x[col_name].i()));
            else                          values.push_back(TupleValue::FromVarchar(x[col_name].s()));
        }
        char tname[MAX_NAME];
        strcpy(tname, table_name.c_str());
        insert_command(tname, values, schema);
        delete meta;
        return crow::response(200, "Insertion triggered (check logs for success/failure)");
    });

    CROW_ROUTE(app, "/bulk_insert/<string>").methods(crow::HTTPMethod::POST)
    ([&is_authenticated](const crow::request& req, std::string table_name) {
        if (!is_authenticated(req)) return crow::response(401, "Authentication required");
        auto x = crow::json::load(req.body);
        if (!x || x.size() == 0) return crow::response(400, "Invalid JSON or empty array");

        struct table* meta = fetch_meta_data(table_name);
        if (!meta) return crow::response(404, "Table not found");

        std::vector<ColumnSchema> schema;
        for (int i = 0; i < meta->count; i++) {
            schema.push_back(ColumnSchema(meta->col[i].col_name,
                                         meta->col[i].type == INT ? STORAGE_COLUMN_INT : STORAGE_COLUMN_VARCHAR,
                                         meta->col[i].size));
        }
        std::vector<std::vector<TupleValue>> all_values;
        for (auto& row : x) {
            std::vector<TupleValue> values;
            bool valid_row = true;
            for (int i = 0; i < meta->count; i++) {
                std::string col_name = meta->col[i].col_name;
                if (!row.has(col_name)) { valid_row = false; break; }
                if (meta->col[i].type == INT) values.push_back(TupleValue::FromInt(row[col_name].i()));
                else                          values.push_back(TupleValue::FromVarchar(row[col_name].s()));
            }
            if (valid_row) all_values.push_back(values);
        }
        char tname[MAX_NAME];
        strcpy(tname, table_name.c_str());
        bulk_insert_command(tname, all_values, schema);
        delete meta;
        return crow::response(200, "Bulk insertion triggered (check logs for success/failure)");
    });

    CROW_ROUTE(app, "/health")
    ([]() { return "MiniDB API is running!"; });

    // ---------------------------------------------------------------------------
    // /query — detects SELECT and returns structured JSON; DML returns a message
    // ---------------------------------------------------------------------------
    CROW_ROUTE(app, "/query").methods(crow::HTTPMethod::POST)
    ([&is_authenticated](const crow::request& req) {
        if (!is_authenticated(req)) return crow::response(401, "Authentication required");

        auto x = crow::json::load(req.body);
        if (!x || !x.has("query")) return crow::response(400, "Invalid JSON: 'query' field required");

        std::string query = x["query"].s();

        // Determine first keyword
        std::string first_word;
        bool started = false;
        for (char c : query) {
            if (!started && isspace((unsigned char)c)) continue;
            started = true;
            if (isspace((unsigned char)c) || c == ';') break;
            first_word += static_cast<char>(tolower((unsigned char)c));
        }

        if (first_word == "select") {
            // Return full structured result
            auto result = handle_select_api(query);
            return crow::response(result);
        } else {
            // DML: CREATE, INSERT, UPDATE, DROP, SHOW — execute and return message
            execute_query_string(query);
            crow::json::wvalue res;
            res["type"]    = "dml";
            res["message"] = "Query executed successfully";
            return crow::response(res);
        }
    });

    app.port(18080).multithreaded().run();
}
