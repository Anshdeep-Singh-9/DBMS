#ifndef DISPLAY_H
#define DISPLAY_H

#include "declaration.h"
#include <vector>
#include <string>
#include <map>

extern void process_select(std::vector <std::string> &token_vector);
extern void display_meta_data();
extern void display(); // New function for option 5
extern int search_table(char tab_name[]);

#endif
