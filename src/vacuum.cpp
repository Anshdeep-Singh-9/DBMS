#include "vacuum.h"
#include "storage_types.h"
#include "data_page.h"
#include <cstring> // Required for std::memcpy

void compact_page(uint32_t page_id, DiskManager* disk) {
    char old_buffer[STORAGE_PAGE_SIZE];
    char new_buffer[STORAGE_PAGE_SIZE] = {0}; // The clean shadow page

    // 1. Fetch from the physical disk
    if (!disk->read_page(page_id, old_buffer)) {
        return; // Page doesn't exist or read failed
    }

    // 2. Cast the raw bytes to a Header struct
    PageHeader* old_header = reinterpret_cast<PageHeader*>(old_buffer);
    PageHeader* new_header = reinterpret_cast<PageHeader*>(new_buffer);

    // 3. Copy the header and the ENTIRE slot array (including dead tombstones)
    // Your teammate's `free_start` pointer tracks the exact boundary where 
    // the slot array ends. We can copy the header and slots in one elegant move!
    std::memcpy(new_buffer, old_buffer, old_header->free_start);

    // 4. Reset the tuple insertion pointer to the very bottom of the new page
    new_header->free_end = STORAGE_PAGE_SIZE;

    // 5. Point to the start of the slot arrays
    SlotEntry* old_slots = reinterpret_cast<SlotEntry*>(old_buffer + sizeof(PageHeader));
    SlotEntry* new_slots = reinterpret_cast<SlotEntry*>(new_buffer + sizeof(PageHeader));

    // 6. The Compaction Loop
    for (int i = 0; i < new_header->slot_count; i++) {
        // If the slot is ALIVE (assuming length > 0 means it hasn't been deleted)
        if (new_slots[i].length > 0) {
            
            // Calculate new destination at the bottom of the new page
            new_header->free_end -= new_slots[i].length;
            
            // Copy the actual row data from the old page to the new destination
            std::memcpy(
                new_buffer + new_header->free_end, // Destination
                old_buffer + old_slots[i].offset,  // Source
                new_slots[i].length                // How many bytes
            );

            // Update the slot directory to point to the new compacted location
            new_slots[i].offset = new_header->free_end;
        }
    }

    // 7. Overwrite the fragmented disk page with the fully compacted one
    disk->write_page(page_id, new_buffer);
}