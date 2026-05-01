#ifndef DISK_MANAGER_H
#define DISK_MANAGER_H

#include <fstream>
#include <string>
#include "storage_types.h" // Gives us access to TableHeader and STORAGE_PAGE_SIZE

class DiskManager {
public:
    DiskManager(const std::string& path, std::size_t page_size = STORAGE_PAGE_SIZE);
    ~DiskManager();

    bool open_or_create();
    void close();

    uint32_t allocate_page();
    void deallocate_page(uint32_t page_id);

    bool read_page(uint32_t page_id, char* buffer);
    bool write_page(uint32_t page_id, const char* buffer);

    uint32_t page_count();
    std::size_t page_size() const;
    bool is_open() const;

private:
    std::fstream file_;
    std::string path_;
    std::size_t page_size_;

    // The old RAM-based vector (std::vector<uint32_t> free_pages_) has been DELETED.
    // Free pages are now strictly tracked via the Page 0 Bitmap.

    uint32_t page_count_unchecked();
    std::streamoff page_offset(uint32_t page_id) const;

    // --- NEW: Bitwise Math Helpers for the Page 0 Bitmap ---
    // These are 'inline' so the compiler optimizes them for maximum speed.

    inline void set_bit(uint8_t* bitmap, uint32_t page_id) {
        uint32_t byte_index = page_id / 8;
        uint32_t bit_index  = page_id % 8;
        bitmap[byte_index] |= (1 << bit_index); // Mark as 1 (Full)
    }

    inline void clear_bit(uint8_t* bitmap, uint32_t page_id) {
        uint32_t byte_index = page_id / 8;
        uint32_t bit_index  = page_id % 8;
        bitmap[byte_index] &= ~(1 << bit_index); // Mark as 0 (Empty)
    }

    inline bool is_page_full(const uint8_t* bitmap, uint32_t page_id) const {
        uint32_t byte_index = page_id / 8;
        uint32_t bit_index  = page_id % 8;
        return (bitmap[byte_index] & (1 << bit_index)) != 0; // Returns true if bit is 1
    }
};

#endif