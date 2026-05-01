#include "recovery_manager.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include "BPtree.h"
#include "disk_manager.h"

namespace fs = std::filesystem;

namespace {

static const uint32_t RECOVERY_LOG_MAGIC = 0x4D444257;  // "WBDM" in hex order
static const uint32_t RECOVERY_LOG_VERSION = 1;
static const uint32_t RECOVERY_LOG_PENDING = 0;
static const uint32_t RECOVERY_LOG_APPLIED = 1;

struct InsertRedoRecord {
    uint32_t magic;
    uint32_t version;
    uint32_t applied;
    char table_name[MAX_NAME];
    uint32_t page_id;
    uint16_t slot_id;
    uint16_t reserved;
    int32_t primary_key;
    uint32_t sync_index;
    uint32_t page_size;
    char page_bytes[STORAGE_PAGE_SIZE];
};

std::string wal_path_for_table(const std::string& table_name) {
    return "table/" + table_name + "/wal.log";
}

bool ensure_wal_parent_exists(const std::string& table_name) {
    const fs::path table_dir = fs::path("table") / table_name;
    if (fs::exists(table_dir)) {
        return true;
    }
    return false;
}

bool open_wal_rw(const std::string& path, std::fstream& file) {
    file.open(path.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    if (file.is_open()) {
        return true;
    }

    std::ofstream create(path.c_str(), std::ios::out | std::ios::binary);
    if (!create.is_open()) {
        return false;
    }
    create.close();

    file.open(path.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    return file.is_open();
}

bool is_valid_record(const InsertRedoRecord& record) {
    return record.magic == RECOVERY_LOG_MAGIC &&
           record.version == RECOVERY_LOG_VERSION &&
           record.page_size == STORAGE_PAGE_SIZE &&
           record.table_name[0] != '\0';
}

bool redo_record(const InsertRedoRecord& record) {
    std::string data_path = "table/" + std::string(record.table_name) + "/data.dat";
    DiskManager data_disk(data_path, STORAGE_PAGE_SIZE);
    if (!data_disk.open_or_create()) {
        return false;
    }

    while (data_disk.page_count() <= record.page_id) {
        if (data_disk.allocate_page() == INVALID_PAGE_ID) {
            return false;
        }
    }

    if (!data_disk.write_page(record.page_id, record.page_bytes)) {
        return false;
    }

    BPtree index(record.table_name);
    RID rid(record.page_id, record.slot_id);
    if (record.sync_index != 0) {
        RID existing = index.search(record.primary_key);
        if (existing.page_id == INVALID_PAGE_ID) {
            index.insert(record.primary_key, rid);
        } else if (existing.page_id != rid.page_id || existing.slot_id != rid.slot_id) {
            index.update_rid(record.primary_key, rid);
        }
    }

    return true;
}

}  // namespace

RecoveryTicket RecoveryManager::log_page_redo(const std::string& table_name,
                                              uint32_t page_id,
                                              uint16_t slot_id,
                                              int primary_key,
                                              const char* page_bytes,
                                              bool sync_index) {
    if (page_bytes == NULL || !ensure_wal_parent_exists(table_name)) {
        return RecoveryTicket();
    }

    InsertRedoRecord record;
    std::memset(&record, 0, sizeof(record));
    record.magic = RECOVERY_LOG_MAGIC;
    record.version = RECOVERY_LOG_VERSION;
    record.applied = RECOVERY_LOG_PENDING;
    std::strncpy(record.table_name, table_name.c_str(), MAX_NAME - 1);
    record.page_id = page_id;
    record.slot_id = slot_id;
    record.primary_key = primary_key;
    record.sync_index = sync_index ? 1U : 0U;
    record.page_size = STORAGE_PAGE_SIZE;
    std::memcpy(record.page_bytes, page_bytes, STORAGE_PAGE_SIZE);

    const std::string wal_path = wal_path_for_table(table_name);
    std::fstream wal_file;
    if (!open_wal_rw(wal_path, wal_file)) {
        return RecoveryTicket();
    }

    wal_file.clear();
    wal_file.seekp(0, std::ios::end);
    const std::uint64_t record_offset =
        static_cast<std::uint64_t>(wal_file.tellp());
    wal_file.write(reinterpret_cast<const char*>(&record), sizeof(record));
    wal_file.flush();

    if (!wal_file.good()) {
        return RecoveryTicket();
    }

    return RecoveryTicket(table_name, record_offset);
}

RecoveryTicket RecoveryManager::log_insert_redo(const std::string& table_name,
                                                uint32_t page_id,
                                                uint16_t slot_id,
                                                int primary_key,
                                                const char* page_bytes) {
    return log_page_redo(table_name, page_id, slot_id, primary_key, page_bytes, true);
}

bool RecoveryManager::mark_page_applied(const RecoveryTicket& ticket) {
    if (!ticket.valid) {
        return false;
    }

    const std::string wal_path = wal_path_for_table(ticket.table_name);
    std::fstream wal_file;
    if (!open_wal_rw(wal_path, wal_file)) {
        return false;
    }

    const std::streamoff applied_offset =
        static_cast<std::streamoff>(ticket.record_offset + offsetof(InsertRedoRecord, applied));
    const uint32_t applied = RECOVERY_LOG_APPLIED;

    wal_file.clear();
    wal_file.seekp(applied_offset, std::ios::beg);
    wal_file.write(reinterpret_cast<const char*>(&applied), sizeof(applied));
    wal_file.flush();

    return wal_file.good();
}

bool RecoveryManager::mark_insert_applied(const RecoveryTicket& ticket) {
    return mark_page_applied(ticket);
}

bool RecoveryManager::recover_all_tables() {
    const fs::path table_root("table");
    if (!fs::exists(table_root) || !fs::is_directory(table_root)) {
        return true;
    }

    bool success = true;
    for (fs::directory_iterator it(table_root); it != fs::directory_iterator(); ++it) {
        const fs::path entry = it->path();
        if (!fs::is_directory(entry)) {
            continue;
        }

        const fs::path meta_path = entry / "met";
        if (!fs::exists(meta_path)) {
            continue;
        }

        const std::string table_name = entry.filename().string();
        if (!recover_table(table_name)) {
            success = false;
        }
    }

    return success;
}

bool RecoveryManager::recover_table(const std::string& table_name) {
    const std::string wal_path = wal_path_for_table(table_name);
    if (!fs::exists(wal_path)) {
        return true;
    }

    std::ifstream wal_file(wal_path.c_str(), std::ios::in | std::ios::binary);
    if (!wal_file.is_open()) {
        return false;
    }

    std::vector<InsertRedoRecord> pending_records;

    while (true) {
        InsertRedoRecord record;
        wal_file.read(reinterpret_cast<char*>(&record), sizeof(record));
        if (wal_file.gcount() == 0) {
            break;
        }

        if (!wal_file || wal_file.gcount() != static_cast<std::streamsize>(sizeof(record))) {
            break;
        }

        if (!is_valid_record(record)) {
            continue;
        }

        if (record.applied == RECOVERY_LOG_PENDING) {
            pending_records.push_back(record);
        }
    }

    wal_file.close();

    for (std::size_t i = 0; i < pending_records.size(); ++i) {
        if (!redo_record(pending_records[i])) {
            return false;
        }
    }

    std::ofstream truncate_file(wal_path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    truncate_file.close();
    return true;
}

void RecoveryManager::maybe_crash_after_wal(const char* operation) {
    const char* failpoint = std::getenv("MINIDB_CRASH_AFTER_WAL");
    if (failpoint == NULL || operation == NULL) {
        return;
    }
    if (std::string(failpoint) == operation) {
        std::abort();
    }
}
