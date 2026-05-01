#ifndef VACUUM_H
#define VACUUM_H

#include <cstdint>
#include "disk_manager.h"

// Note: Include the file where PageHeader and Slot are defined. 
// Based on your file tree, it is likely inside data_page.h or storage_types.h
#include "data_page.h" 
#include "storage_types.h"

void compact_page(uint32_t page_id, DiskManager* disk);

#endif // VACUUM_H