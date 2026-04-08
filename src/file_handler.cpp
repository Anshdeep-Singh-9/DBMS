
#include "file_handler.h"
namespace fs= std::filesystem;

std::fstream open_file(char t_name[] ,std::ios::openmode mode){

    fs::path file_path;

    fs::path exe_dir=fs::canonical("/proc/self/exe").parent_path();

    file_path= exe_dir.parent_path().parent_path() / "table";

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

    // return std::move(file);
    return file;
}

FILE *open_file(char t_name[] ,char perm[]){

    FILE *fp;
    struct stat st = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    char *name = (char *)malloc(sizeof(char) * (2 * MAX_NAME + 15));
       strcpy(name, "./table/");

    if (stat(name, &st) == -1)
        mkdir(name, 0775);

    strcat(name, t_name);
    strcat(name, "/");

    if (stat(name, &st) == -1)
        mkdir(name, 0775);
    strcat(name, "met");
    fp = fopen(name, perm);
    if (!fp){
        printf("\nError in opening file\n");
    }
    free(name);
    return fp;
}

int store_meta_data(struct table *t_ptr)
{
    FILE *fp = open_file(t_ptr->name, const_cast<char*>("w"));
    fwrite(t_ptr, sizeof(struct table), 1, fp);
    fclose(fp);
    return 0;
}

/* Can't be used right now.*/

void store_meta_data_fstream(struct table *t_ptr)
{
    std::fstream fp = open_file(t_ptr->name, std::ios::out | std::ios::trunc | std::ios::binary);

    fp.write((char *)t_ptr, sizeof(struct table));
    fp.close();

    // return 0;
}


struct table* fetch_meta_data(string name){
    char* name_ptr=&name[0];
    std::fstream fp=open_file(name_ptr, READ_BIN);

    struct table* t=new table();
    
    fp.read((char*)t, sizeof(struct table));
    
    // Verify read was successful and we got the right amount of data
    if(fp.fail() || fp.gcount() != (std::streamsize)sizeof(struct table)){
        cout<<"ERROR! Failed to read metadata for table: "<<name<<endl;
        delete t;
        fp.close();
        return NULL;
    }
    
    fp.close();
    return t;
}

// void display_meta_data(struct table* t_ptr){
//     cout<<"Table Name: "<<t_ptr->name<<endl<<endl;
//     cout<<"Table Attributes:\n";
//     for(int i=0; i<t_ptr->count; i++){
//         cout<<"name: "<<t_ptr->col[i].col_name<<endl;
//         cout<<"type: "<<((t_ptr->col[i].type == 1) ? "INT" : "VARCHAR")<<endl;
//         cout<<"size: "<<t_ptr->col[i].size<<endl<<endl;
//     }
// }

// int main(){
//     struct table* t;
//     t=fetch_meta_data("user");
//     display_meta_data(t);
//     delete t;
// }
