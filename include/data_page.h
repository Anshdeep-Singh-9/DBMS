#ifndef DATA_PAGE_H
#define DATA_PAGE_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "storage_types.h"

/*
 * DataPage is the next layer above DiskManager.
 *
 * Before:
 * - the old code wrote one row directly into one separate file
 *
 * After:
 * - many rows are packed into one page
 * - each inserted row gets a slot_id inside that page
 *
 * Layman version:
 * - DiskManager handles the notebook
 * - DataPage decides where each line is written on a page of that notebook
 */
class DataPage {
  public:
    DataPage();

    void initialize(uint32_t page_id);

    bool load_from_buffer(const char* buffer, std::size_t buffer_size);
    const char* data() const;
    char* data();
    std::size_t size() const;

    PageHeader header() const;
    bool write_header(const PageHeader& header);

    uint16_t free_space() const;
    uint16_t slot_count() const;
    bool can_store(std::size_t tuple_size) const;

    bool insert_tuple(const char* tuple_data,
                      uint16_t tuple_size,
                      uint16_t& slot_id_out);
    bool read_tuple(uint16_t slot_id, std::vector<char>& tuple_out) const;

  private:
    std::vector<char> bytes_;

    SlotEntry read_slot(uint16_t slot_id) const;
    void write_slot(uint16_t slot_id, const SlotEntry& entry);
};

#endif
