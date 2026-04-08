#ifndef STORAGE_TYPES_H
#define STORAGE_TYPES_H

#include <cstddef>
#include <cstdint>

/*
 * Storage foundation for the redesigned engine.
 *
 * Before:
 * - one inserted row was effectively tracked as fileN.dat
 * - the index pointed to a record number that later opened a row file
 *
 * After:
 * - data is meant to live inside fixed-size pages
 * - rows are identified by RID(page_id, slot_id)
 * - higher layers will fetch a page first, then a tuple inside that page
 *
 * Layman version:
 * - before, one row behaved like one small file
 * - after, many rows live together inside one bigger block called a page
 */

static const std::size_t STORAGE_PAGE_SIZE = 4096;
static const uint32_t INVALID_PAGE_ID = UINT32_MAX;
static const uint16_t INVALID_SLOT_ID = UINT16_MAX;

enum PageType {
    PAGE_TYPE_INVALID = 0,
    PAGE_TYPE_TABLE_DATA = 1,
    PAGE_TYPE_BTREE_INTERNAL = 2,
    PAGE_TYPE_BTREE_LEAF = 3
};

/*
 * RID is the new logical row address.
 *
 * Why this replaces record_number:
 * - it matches page-based storage
 * - it is what a B+ Tree should point to later
 * - it avoids coupling lookup logic to file names like file7.dat
 *
 * Layman version:
 * - before, we said "open row file number 7"
 * - after, we say "go to page X and slot Y"
 */
struct RID {
    uint32_t page_id;
    uint16_t slot_id;

    RID()
        : page_id(INVALID_PAGE_ID), slot_id(INVALID_SLOT_ID) {
    }

    RID(uint32_t page, uint16_t slot)
        : page_id(page), slot_id(slot) {
    }
};

/*
 * Each slot entry tells us where one tuple lives inside a page.
 *
 * offset -> starting byte of the tuple
 * length -> tuple size in bytes
 *
 * Layman version:
 * - this is a small label telling us where a row starts and how big it is
 */
struct SlotEntry {
    uint16_t offset;
    uint16_t length;

    SlotEntry()
        : offset(0), length(0) {
    }

    SlotEntry(uint16_t tuple_offset, uint16_t tuple_length)
        : offset(tuple_offset), length(tuple_length) {
    }
};

/*
 * Header placed at the beginning of each page.
 *
 * free_start grows forward as slot entries are added.
 * free_end grows backward as tuple bytes are inserted.
 * The free space of the page always stays between them.
 *
 * Layman version:
 * - the header is the control panel of the page
 * - it tells us how full the page is and where the next row can go
 */
struct PageHeader {
    uint32_t page_id;
    uint16_t slot_count;
    uint16_t free_start;
    uint16_t free_end;
    uint16_t page_type;
    uint32_t next_page_id;

    PageHeader()
        : page_id(INVALID_PAGE_ID),
          slot_count(0),
          free_start(sizeof(PageHeader)),
          free_end(static_cast<uint16_t>(STORAGE_PAGE_SIZE)),
          page_type(PAGE_TYPE_INVALID),
          next_page_id(INVALID_PAGE_ID) {
    }
};

#endif
