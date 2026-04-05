/*
// ----------------------------------------------------------------------
 // File    : file_handler.cpp
 // Author  : Mandeep singh
 // Purpose : code for creating new files and folders (file handling operations)
 //
 //
 // DeepDataBase, Copyright (C) 2015 - 2017
 //
 // This program is free software; you can redistribute it and/or
 // modify it under the terms of the GNU General Public License as
 // published by the Free Software Foundation; either version 2 of the
 // License, or (at your option) any later version.
 //
 // This program is distributed in the hope that it will be useful,
 // but WITHOUT ANY WARRANTY; without even the implied warranty of
 // MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 // General Public License for more details.
 //
 // You should have received a copy of the GNU General Public License along
 // with this program; if not, write to the Free Software Foundation, Inc.,
 // 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

// ----------------------------------------------------------------------
*/

#include "file_handler.h"
namespace fs= std::filesystem;

std::fstream open_file(char t_name[] ,std::ios::openmode mode){

    fs::path file_path;

    fs::path exe_dir=fs::canonical("/proc/self/exe").parent_path();

    file_path= exe_dir / "table";

    if(!fs::exists(file_path)) fs::create_directories(file_path);

    file_path=file_path / t_name ;

    if(!fs::exists(file_path)) fs::create_directory(file_path);

    file_path=file_path / "met";

    if(!fs::exists(file_path)){
        std::fstream newfile(file_path, std::ios::out | std::ios::binary);  // creates the file
        newfile.close();
    }

    std::fstream file(file_path, mode);

    if(!file){
        cout<<"Error opening file!"<<endl;
        exit(0);
    }

    /*
    "r"  → std::ios::in
    "w"  → std::ios::out | std::ios::trunc
    "w+" → std::ios::in | std::ios::out | std::ios::trunc
    "r+" → std::ios::in | std::ios::out
    "a"  → std::ios::app
    */

    return std::move(file);
}

int store_meta_data(struct table *t_ptr)
{
    std::fstream fp = open_file(t_ptr->name, std::ios::out | std::ios::trunc | std::ios::binary);

    fp.write((char *)t_ptr, sizeof(struct table));
    fp.close();

    return 0;
}

struct table* fetch_meta_data(string name){
    char* name_ptr=&name[0];
    std::fstream fp=open_file(name_ptr, READ_BIN);

    struct table* t=new table();
    fp.read((char*)t, sizeof(struct table));

    return t;
}

void display_meta_data(struct table* t_ptr){
    cout<<"Table Name: "<<t_ptr->name<<endl<<endl;
    cout<<"Table Attributes:\n";
    for(int i=0; i<t_ptr->count; i++){
        cout<<"name: "<<t_ptr->col[i].col_name<<endl;
        cout<<"type: "<<((t_ptr->col[i].type == 1) ? "INT" : "VARCHAR")<<endl;
        cout<<"size: "<<t_ptr->col[i].size<<endl<<endl;
    }
}

// int main(){
//     struct table* t;
//     t=fetch_meta_data("user");
//     display_meta_data(t);
//     delete t;
// }
