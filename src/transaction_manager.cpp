#include "transaction_manager.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

bool TransactionManager::active_ = false;
std::string TransactionManager::error_;
static std::uint64_t g_current_txn_id = 0;
static std::uint64_t g_next_txn_id = 1;

const char* TransactionManager::snapshot_root() {
    return "system/txn_snapshot";
}

bool TransactionManager::remove_snapshot_dir() {
    std::error_code ec;
    fs::remove_all(snapshot_root(), ec);
    return !ec;
}

bool TransactionManager::copy_tables_to_snapshot() {
    std::error_code ec;
    fs::create_directories(snapshot_root(), ec);
    if (ec) {
        error_ = "Could not create transaction snapshot directory.";
        return false;
    }

    const fs::path table_root("table");
    if (!fs::exists(table_root) || !fs::is_directory(table_root)) {
        return true;
    }

    for (fs::directory_iterator it(table_root, ec); !ec && it != fs::directory_iterator(); ++it) {
        const fs::path src = it->path();
        if (!fs::is_directory(src)) continue;

        const fs::path dst = fs::path(snapshot_root()) / src.filename();
        fs::create_directories(dst, ec);
        if (ec) {
            error_ = "Could not create table snapshot directory.";
            return false;
        }

        for (fs::directory_iterator fit(src, ec); !ec && fit != fs::directory_iterator(); ++fit) {
            const fs::path fsrc = fit->path();
            if (!fs::is_regular_file(fsrc)) continue;
            fs::copy_file(fsrc, dst / fsrc.filename(), fs::copy_options::overwrite_existing, ec);
            if (ec) {
                error_ = "Could not copy table files to snapshot.";
                return false;
            }
        }
        if (ec) {
            error_ = "Failed while reading table directory.";
            return false;
        }
    }
    if (ec) {
        error_ = "Failed while scanning tables.";
        return false;
    }

    const fs::path list_path("table/table_list");
    if (fs::exists(list_path)) {
        fs::copy_file(list_path, fs::path(snapshot_root()) / "table_list",
                      fs::copy_options::overwrite_existing, ec);
        if (ec) {
            error_ = "Could not snapshot table_list.";
            return false;
        }
    }

    return true;
}

bool TransactionManager::restore_snapshot_to_tables() {
    std::error_code ec;
    const fs::path table_root("table");
    const fs::path snap_root(snapshot_root());
    if (!fs::exists(snap_root) || !fs::is_directory(snap_root)) {
        error_ = "No transaction snapshot found for rollback.";
        return false;
    }

    for (fs::directory_iterator it(table_root, ec); !ec && it != fs::directory_iterator(); ++it) {
        const fs::path p = it->path();
        if (fs::is_directory(p) && p.filename() != "table_list") {
            fs::remove_all(p, ec);
            if (ec) {
                error_ = "Could not clear current table state.";
                return false;
            }
        }
    }

    for (fs::directory_iterator it(snap_root, ec); !ec && it != fs::directory_iterator(); ++it) {
        const fs::path src = it->path();
        if (src.filename() == "table_list") continue;
        if (!fs::is_directory(src)) continue;

        const fs::path dst = table_root / src.filename();
        fs::create_directories(dst, ec);
        if (ec) {
            error_ = "Could not recreate table directory.";
            return false;
        }

        for (fs::directory_iterator fit(src, ec); !ec && fit != fs::directory_iterator(); ++fit) {
            const fs::path fsrc = fit->path();
            if (!fs::is_regular_file(fsrc)) continue;
            fs::copy_file(fsrc, dst / fsrc.filename(), fs::copy_options::overwrite_existing, ec);
            if (ec) {
                error_ = "Could not restore table files from snapshot.";
                return false;
            }
        }
        if (ec) {
            error_ = "Failed while restoring table directory.";
            return false;
        }
    }

    const fs::path snap_table_list = snap_root / "table_list";
    if (fs::exists(snap_table_list)) {
        fs::copy_file(snap_table_list, table_root / "table_list",
                      fs::copy_options::overwrite_existing, ec);
        if (ec) {
            error_ = "Could not restore table_list.";
            return false;
        }
    }

    return true;
}

bool TransactionManager::begin() {
    if (active_) {
        error_ = "A transaction is already active.";
        return false;
    }

    error_.clear();
    if (!remove_snapshot_dir()) {
        error_ = "Could not reset old transaction snapshot directory.";
        return false;
    }
    if (!copy_tables_to_snapshot()) {
        remove_snapshot_dir();
        return false;
    }

    active_ = true;
    g_current_txn_id = g_next_txn_id++;
    return true;
}

bool TransactionManager::commit() {
    if (!active_) {
        error_ = "No active transaction to commit.";
        return false;
    }

    active_ = false;
    g_current_txn_id = 0;
    if (!remove_snapshot_dir()) {
        error_ = "Committed, but failed to clear snapshot directory.";
        return false;
    }

    return true;
}

bool TransactionManager::rollback() {
    if (!active_) {
        error_ = "No active transaction to rollback.";
        return false;
    }

    if (!restore_snapshot_to_tables()) {
        return false;
    }

    active_ = false;
    g_current_txn_id = 0;
    if (!remove_snapshot_dir()) {
        error_ = "Rolled back, but failed to clear snapshot directory.";
        return false;
    }

    return true;
}

bool TransactionManager::in_transaction() {
    return active_;
}

std::uint64_t TransactionManager::current_txn_id() {
    return g_current_txn_id;
}

std::string TransactionManager::last_error() {
    return error_;
}
