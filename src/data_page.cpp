#include "data_page.h"

#include <cstring>

namespace {
const std::size_t SLOT_ENTRY_SIZE = sizeof(SlotEntry);
}

/*
 * This file turns a raw 4 KB block into a usable table-data page.
 *
 * Before:
 * - storage logic had no concept of "multiple rows inside one page"
 *
 * After:
 * - one page has a header, slot directory, free space, and tuple area
 * - tuples are inserted from the back, slots are added from the front
 *
 * Layman version:
 * - this is the packing logic inside one page
 * - it decides where each row sits inside the block
 */

DataPage::DataPage()
    : bytes_(STORAGE_PAGE_SIZE, 0) {
}

void DataPage::initialize(uint32_t page_id) {
    std::fill(bytes_.begin(), bytes_.end(), 0);

    PageHeader header;
    header.page_id = page_id;
    header.page_type = PAGE_TYPE_TABLE_DATA;
    write_header(header);
}

bool DataPage::load_from_buffer(const char* buffer, std::size_t buffer_size) {
    if (buffer == NULL || buffer_size != bytes_.size()) {
        return false;
    }

    std::memcpy(&bytes_[0], buffer, bytes_.size());
    return true;
}

const char* DataPage::data() const {
    return &bytes_[0];
}

char* DataPage::data() {
    return &bytes_[0];
}

std::size_t DataPage::size() const {
    return bytes_.size();
}

PageHeader DataPage::header() const {
    PageHeader header;
    std::memcpy(&header, &bytes_[0], sizeof(PageHeader));
    return header;
}

bool DataPage::write_header(const PageHeader& header) {
    if (sizeof(PageHeader) > bytes_.size()) {
        return false;
    }

    std::memcpy(&bytes_[0], &header, sizeof(PageHeader));
    return true;
}

uint16_t DataPage::free_space() const {
    const PageHeader page_header = header();
    if (page_header.free_end < page_header.free_start) {
        return 0;
    }

    return static_cast<uint16_t>(page_header.free_end - page_header.free_start);
}

uint16_t DataPage::slot_count() const {
    return header().slot_count;
}

bool DataPage::can_store(std::size_t tuple_size) const {
    return free_space() >= tuple_size + SLOT_ENTRY_SIZE;
}

bool DataPage::insert_tuple(const char* tuple_data,
                            uint16_t tuple_size,
                            uint16_t& slot_id_out) {
    if (tuple_data == NULL || tuple_size == 0 || !can_store(tuple_size)) {
        return false;
    }

    PageHeader page_header = header();

    const uint16_t tuple_offset =
        static_cast<uint16_t>(page_header.free_end - tuple_size);
    std::memcpy(&bytes_[tuple_offset], tuple_data, tuple_size);

    const uint16_t new_slot_id = page_header.slot_count;
    write_slot(new_slot_id, SlotEntry(tuple_offset, tuple_size));

    page_header.slot_count = static_cast<uint16_t>(page_header.slot_count + 1);

    page_header.free_start = static_cast<uint16_t>(sizeof(PageHeader) + page_header.slot_count * SLOT_ENTRY_SIZE);
    
    page_header.free_end = tuple_offset;
    write_header(page_header);

    slot_id_out = new_slot_id;
    return true;
}

bool DataPage::read_tuple(uint16_t slot_id, std::vector<char>& tuple_out) const {
    if (slot_id >= slot_count()) {
        return false;
    }

    const SlotEntry entry = read_slot(slot_id);
    if (entry.length == 0) {
        return false;
    }

    tuple_out.assign(entry.length, 0);
    std::memcpy(&tuple_out[0], &bytes_[entry.offset], entry.length);
    return true;
}

SlotEntry DataPage::read_slot(uint16_t slot_id) const {
    SlotEntry entry;
    const std::size_t slot_offset =
        sizeof(PageHeader) + static_cast<std::size_t>(slot_id) * SLOT_ENTRY_SIZE;
    std::memcpy(&entry, &bytes_[slot_offset], SLOT_ENTRY_SIZE);
    return entry;
}

void DataPage::write_slot(uint16_t slot_id, const SlotEntry& entry) {
    const std::size_t slot_offset =
        sizeof(PageHeader) + static_cast<std::size_t>(slot_id) * SLOT_ENTRY_SIZE;
    std::memcpy(&bytes_[slot_offset], &entry, SLOT_ENTRY_SIZE);
}
