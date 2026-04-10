#ifndef DISK_MANAGER_H
#define DISK_MANAGER_H

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>

#include "storage_types.h"

/*
 * DiskManager is the raw page I/O layer.
 *
 * It should only know:
 * - where the file is
 * - how large a page is
 * - how to allocate, read, and write a page
 *
 * It should not know:
 * - SQL
 * - tuples
 * - indexes
 * - query execution
 *
 * That separation is intentional because this layer is the base for
 * both table pages and future index pages.
 *
 * Layman version:
 * - this class is just the page file handler
 * - it should only know how to store and fetch page-sized blocks
 */
class DiskManager {
  public:
    explicit DiskManager(const std::string& path,
                         std::size_t page_size = STORAGE_PAGE_SIZE);
    ~DiskManager();

    bool open_or_create();
    void close();

    uint32_t allocate_page();
    bool read_page(uint32_t page_id, char* buffer);
    bool write_page(uint32_t page_id, const char* buffer);

    uint32_t page_count();
    std::size_t page_size() const;
    bool is_open() const;

  private:
    std::string path_;
    std::size_t page_size_;
    std::fstream file_;

    uint32_t page_count_unchecked();
    std::streamoff page_offset(uint32_t page_id) const;
};

#endif
