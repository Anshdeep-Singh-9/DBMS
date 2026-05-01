#include "disk_manager.h"
#include <vector>
#include <iostream>

/*
 * This file implements the new page-based disk access path.
 *
 * Before:
 * - old code directly opened metadata files and per-row files
 * - recycled pages were lost when the RAM vector cleared.
 *
 * After:
 * - one storage file contains equal-sized blocks.
 * - Page 0 is strictly reserved for the TableHeader and Free Space Bitmap.
 */

DiskManager::DiskManager(const std::string& path, std::size_t page_size)
    : path_(path), page_size_(page_size) {
}

DiskManager::~DiskManager() {
    close();
}

bool DiskManager::open_or_create() {
    close();

    bool is_new_file = false;

    // Try opening an existing database file first.
    file_.open(path_.c_str(), std::ios::in | std::ios::out | std::ios::binary);
    
    if (!file_.is_open()) {
        // If it does not exist yet, create an empty file.
        std::ofstream create_file(path_.c_str(), std::ios::out | std::ios::binary);
        if (!create_file.is_open()) {
            return false;
        }
        create_file.close();

        file_.open(path_.c_str(), std::ios::in | std::ios::out | std::ios::binary);
        if (!file_.is_open()) return false;
        
        is_new_file = true; // Flag so we know to format Page 0
    }

    // --- NEW: THE PAGE 0 BOOT SEQUENCE ---
    if (is_new_file) {
        // 1. Format a brand new Page 0
        std::vector<char> zero_page(STORAGE_PAGE_SIZE, 0);
        TableHeader* header = reinterpret_cast<TableHeader*>(&zero_page[0]);
        
        header->magic_number = HEMDB_MAGIC_NUMBER;
        header->version = 1;
        header->page_size = static_cast<uint32_t>(page_size_);
        header->root_page_id = 0; // No B-Tree yet
        // The bitmap is already 0 (Free) because of the vector initialization.

        // Write Page 0 directly to the physical disk
        file_.clear();
        file_.seekp(0, std::ios::beg);
        file_.write(&zero_page[0], STORAGE_PAGE_SIZE);
        file_.flush();
    } else {
        // 2. Validate an existing database file
        std::vector<char> page_zero(STORAGE_PAGE_SIZE, 0);
        file_.clear();
        file_.seekg(0, std::ios::beg);
        file_.read(&page_zero[0], STORAGE_PAGE_SIZE);

        TableHeader* header = reinterpret_cast<TableHeader*>(&page_zero[0]);
        
        // Security Bouncer: Reject non-HEMDB files
        if (header->magic_number != HEMDB_MAGIC_NUMBER) {
            std::cerr << "FATAL: Not a valid Hemant DB file (Magic Number Mismatch)!" << std::endl;
            close();
            return false;
        }
    }

    return true;
}

void DiskManager::close() {
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

uint32_t DiskManager::allocate_page() {
    if (!is_open()) {
        return INVALID_PAGE_ID;
    }

    // Read Page 0 to access the Bitmap
    std::vector<char> page_zero(STORAGE_PAGE_SIZE, 0);
    if (!read_page(0, &page_zero[0])) {
        return INVALID_PAGE_ID;
    }
    
    TableHeader* header = reinterpret_cast<TableHeader*>(&page_zero[0]);
    uint32_t total_pages = page_count_unchecked();

    // 1. FREE LIST RECLAIM (Scan the Bitmap)
    // We start scanning at i = 1, because Page 0 is VIP (Header), never user data.
    for (uint32_t i = 1; i < total_pages; i++) {
        if (!is_page_full(header->free_page_bitmap, i)) {
            // Found a recycled page! Mark it as full (1)
            set_bit(header->free_page_bitmap, i);
            write_page(0, &page_zero[0]); // Save the updated bitmap to disk

            // Wipe the recycled page with zeros for security
            std::vector<char> empty_page(page_size_, 0);
            write_page(i, &empty_page[0]);
            
            return i;
        }
    }

    // 2. PHYSICAL EXPANSION
    const uint32_t new_page_id = total_pages;
    
    // Safety limit: Our 4000-byte bitmap can only track 32,000 pages.
    if (new_page_id >= 32000) {
        std::cerr << "FATAL: Database reached maximum capacity (32,000 pages)!" << std::endl;
        return INVALID_PAGE_ID; 
    }

    // Mark the completely new page as full (1) in the bitmap
    set_bit(header->free_page_bitmap, new_page_id);
    write_page(0, &page_zero[0]); // Save the updated bitmap to disk

    // Append the new blank page to the end of the file
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

void DiskManager::deallocate_page(uint32_t page_id) {
    // Edge Case: Never delete Page 0, and don't delete out of bounds pages!
    if (page_id == 0 || page_id >= page_count_unchecked()) {
        return; 
    }

    // Fetch Page 0
    std::vector<char> page_zero(STORAGE_PAGE_SIZE, 0);
    if (read_page(0, &page_zero[0])) {
        TableHeader* header = reinterpret_cast<TableHeader*>(&page_zero[0]);
        
        // Flip the bit to 0 (Empty) to mark it for recycling
        clear_bit(header->free_page_bitmap, page_id);
        
        // Save the updated bitmap to disk
        write_page(0, &page_zero[0]);
    }
}

bool DiskManager::read_page(uint32_t page_id, char* buffer) {
    if (!is_open() || buffer == NULL || page_id >= page_count_unchecked()) {
        return false;
    }

    file_.clear();
    file_.seekg(page_offset(page_id), std::ios::beg);
    file_.read(buffer, static_cast<std::streamsize>(page_size_));

    return file_.good();
}

bool DiskManager::write_page(uint32_t page_id, const char* buffer) {
    if (!is_open() || buffer == NULL || page_id >= page_count_unchecked()) {
        return false;
    }

    file_.clear();
    file_.seekp(page_offset(page_id), std::ios::beg);
    file_.write(buffer, static_cast<std::streamsize>(page_size_));
    file_.flush();

    return file_.good();
}

uint32_t DiskManager::page_count() {
    if (!is_open()) return 0;
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

    if (bytes <= 0) return 0;

    return static_cast<uint32_t>(bytes / static_cast<std::streamoff>(page_size_));
}

std::streamoff DiskManager::page_offset(uint32_t page_id) const {
    return static_cast<std::streamoff>(page_id) *
           static_cast<std::streamoff>(page_size_);
}