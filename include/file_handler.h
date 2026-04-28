#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#include "declaration.h"
#include <filesystem>

// --- FSTREAM VERSIONS (Renamed to prevent overload ambiguity) ---
extern fstream_t open_file_fstream(char* t_name, std::ios::openmode mode); 
extern fstream_t open_file_read_fstream(char* t_name, std::ios::openmode mode);

// --- FILEPTR VERSIONS ---
extern FilePtr open_file(char t_name[], const char* perm); 
extern FilePtr open_file_read(char t_name[], const char* perm);

extern void store_meta_data_fstream(struct table *t_ptr);
extern int store_meta_data(struct table *t_ptr);
extern struct table* fetch_meta_data(string name);
extern void system_check();

// --- SYSTEM FILE OPERATIONS ---
extern FilePtr open_system_file(const char* t_name, const char* perm);
extern FilePtr open_system_file_read(const char* t_name, const char* perm);
extern struct table* fetch_system_meta_data(string name);
extern int store_system_meta_data(struct table *t_ptr);

const std::ios::openmode READ_BIN   = std::ios::in | std::ios::binary;
const std::ios::openmode WRITE_BIN  = std::ios::out | std::ios::trunc | std::ios::binary;
const std::ios::openmode RW_BIN     = std::ios::in | std::ios::out | std::ios::binary;

#endif