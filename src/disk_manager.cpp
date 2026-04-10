#include "disk_manager.h"

#include <vector>

/*
 * This file implements the new page-based disk access path.
 *
 * Before:
 * - old code directly opened metadata files and per-row files
 *
 * After:
 * - one storage file can contain many fixed-size pages
 * - higher layers will ask for page_id instead of fileN.dat
 *
 * Layman version:
 * - before, storage was scattered across many small files
 * - after, storage is organized into equal-sized blocks inside one file
 */

DiskManager::DiskManager(const std::string& path, std::size_t page_size)
    : path_(path), page_size_(page_size) {
}

DiskManager::~DiskManager() {
    close();
}

bool DiskManager::open_or_create() {
    close();

    // Try opening an existing database file first.
    file_.open(path_.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    if (file_.is_open()) {
        return true;
    }

    // If it does not exist yet, create an empty file and reopen it for r/w.
    std::ofstream create_file(path_.c_str(), std::ios::out | std::ios::binary);
    if (!create_file.is_open()) {
        return false;
    }
    create_file.close();

    file_.open(path_.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    return file_.is_open();
}

void DiskManager::close() {
    if (file_.is_open()) {
        file_.close();
    }
}

uint32_t DiskManager::allocate_page() {
    if (!is_open()) {
        return INVALID_PAGE_ID;
    }

    // New pages are appended at the end of the file.
    // page_id is therefore just the current number of stored pages.
    // Layman version: a new page is simply added after the last existing page.
    const uint32_t new_page_id = page_count_unchecked();
    std::vector<char> empty_page(page_size_, 0);

    file_.clear();
    file_.seekp(page_offset(new_page_id), std::ios::beg);
    file_.write(&empty_page[0], static_cast<std::streamsize>(empty_page.size()));
    file_.flush();

    if (!file_) {
        return INVALID_PAGE_ID;
    }

    return new_page_id;
}

bool DiskManager::read_page(uint32_t page_id, char* buffer) {
    if (!is_open() || buffer == NULL || page_id >= page_count_unchecked()) {
        return false;
    }

    // A page lives at byte offset: page_id * page_size.
    // Layman version: page 0 starts at byte 0, page 1 starts after one page,
    // page 2 after two pages, and so on.
    file_.clear();
    file_.seekg(page_offset(page_id), std::ios::beg);
    file_.read(buffer, static_cast<std::streamsize>(page_size_));

    return file_.good();
}

bool DiskManager::write_page(uint32_t page_id, const char* buffer) {
    if (!is_open() || buffer == NULL || page_id >= page_count_unchecked()) {
        return false;
    }

    // Overwrite exactly one fixed-size page at its computed offset.
    // Layman version: replace one block, not the whole file.
    file_.clear();
    file_.seekp(page_offset(page_id), std::ios::beg);
    file_.write(buffer, static_cast<std::streamsize>(page_size_));
    file_.flush();

    return file_.good();
}

uint32_t DiskManager::page_count() {
    if (!is_open()) {
        return 0;
    }

    return page_count_unchecked();
}

std::size_t DiskManager::page_size() const {
    return page_size_;
}

bool DiskManager::is_open() const {
    return file_.is_open();
}

uint32_t DiskManager::page_count_unchecked() {
    file_.clear();
    file_.seekg(0, std::ios::end);
    const std::streamoff bytes = file_.tellg();

    if (bytes <= 0) {
        return 0;
    }

    return static_cast<uint32_t>(bytes / static_cast<std::streamoff>(page_size_));
}

std::streamoff DiskManager::page_offset(uint32_t page_id) const {
    // This is the key rule of page-based storage:
    // page N starts at N * page_size bytes inside the file.
    // Layman version: every page has a fixed seat number inside the file.
    return static_cast<std::streamoff>(page_id) *
           static_cast<std::streamoff>(page_size_);
}
