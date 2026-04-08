
#include "declaration.h"
#include <filesystem>

extern std::fstream open_file(char* t_name, std::ios::openmode mode);  // main
extern std::fstream open_file(char* t_name, const char* mode); // overload
extern void store_meta_data_fstream(struct table *t_ptr);
extern int store_meta_data(struct table *t_ptr);
extern struct table* fetch_meta_data(string name);

const std::ios::openmode READ_BIN   = std::ios::in | std::ios::binary;
const std::ios::openmode WRITE_BIN  = std::ios::out | std::ios::trunc | std::ios::binary;
const std::ios::openmode RW_BIN     = std::ios::in | std::ios::out | std::ios::binary;
