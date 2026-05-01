#ifndef VACUUM_H
#define VACUUM_H

#include <cstdint>

#include "disk_manager.h"
#include "storage_types.h"

bool compact_page_buffer(char* page_buffer);
void compact_page(uint32_t page_id, DiskManager* disk);

#endif
