#ifndef RECOVERY_MANAGER_H
#define RECOVERY_MANAGER_H

#include <cstdint>
#include <string>

#include "declaration.h"
#include "storage_types.h"

/*
 * What:
 * RecoveryManager adds a simple write-ahead logging based REDO recovery layer
 * on top of the current page storage engine.
 *
 * Why:
 * Right now pages can be flushed to disk, but a crash between "user asked for
 * insert" and "all related files got updated" can leave data.dat and index.dat
 * out of sync. RecoveryManager keeps a durable redo record so the engine can
 * repair itself on the next startup.
 *
 * Understanding:
 * - Before touching the real data page on disk, we write a recovery record.
 * - That record stores the table name, page id, slot id, primary key, and the
 *   full after-image of the page that should exist after the insert.
 * - If the program crashes, startup recovery scans WAL files and replays any
 *   pending records back into data.dat and the B+ Tree index.
 *
 * Concept used:
 * - write-ahead logging
 * - REDO recovery
 * - crash restart recovery
 * - after-image logging
 *
 * Layman version:
 * - before changing the real notebook, write the intended final page into a
 *   safety diary
 * - if the app dies in the middle, read the diary next time and fix the page
 */

struct RecoveryTicket {
    std::string table_name;
    std::uint64_t record_offset;
    bool valid;

    RecoveryTicket() : table_name(), record_offset(0), valid(false) {}
    RecoveryTicket(const std::string& table, std::uint64_t offset)
        : table_name(table), record_offset(offset), valid(true) {}
};

class RecoveryManager {
  public:
    static RecoveryTicket log_page_redo(const std::string& table_name,
                                        uint32_t page_id,
                                        uint16_t slot_id,
                                        int primary_key,
                                        const char* page_bytes,
                                        bool sync_index);

    static RecoveryTicket log_insert_redo(const std::string& table_name,
                                          uint32_t page_id,
                                          uint16_t slot_id,
                                          int primary_key,
                                          const char* page_bytes);

    static bool mark_page_applied(const RecoveryTicket& ticket);
    static bool mark_insert_applied(const RecoveryTicket& ticket);

    static bool recover_all_tables();
    static bool recover_table(const std::string& table_name);
    static void maybe_crash_after_wal(const char* operation);
};

#endif
