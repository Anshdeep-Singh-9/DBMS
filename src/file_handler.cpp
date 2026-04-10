#include "file_handler.h"
namespace fs = std::filesystem;

// --- FSTREAM FUNCTIONS ---
fstream_t open_file_fstream(const char* t_name ,std::ios::openmode mode){
    fs::path file_path;
    fs::path exe_dir = fs::canonical("/proc/self/exe").parent_path();
    file_path = exe_dir.parent_path().parent_path() / "table";

    if(!fs::exists(file_path)) fs::create_directories(file_path);
    file_path = file_path / t_name ;
    if(!fs::exists(file_path)) fs::create_directory(file_path);
    file_path = file_path / "met";

    if(!fs::exists(file_path)){
        fstream_t newfile(file_path, std::ios::out | std::ios::binary); 
        newfile.close();
    }

    fstream_t file(file_path, mode);
    if(!file){
        cout << "Error opening file!" << endl;
        exit(0);
    }
    return file;
}

fstream_t open_file_read_fstream(const char* t_name ,std::ios::openmode mode){
    fs::path file_path;
    fs::path exe_dir = fs::canonical("/proc/self/exe").parent_path();
    file_path = exe_dir.parent_path().parent_path() / "table" / t_name / "met";
    
    if(!fs::exists(file_path)){
        return fstream_t(); 
    }

    fstream_t file(file_path, mode);
    return file;
}


// --- FILEPTR FUNCTIONS ---
FilePtr open_file(char* t_name , const char* perm){
    FilePtr fp;
    struct stat st = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    char *name = (char *)malloc(sizeof(char) * (2 * MAX_NAME + 15));
    strcpy(name, "./table/");

    if (stat(name, &st) == -1) mkdir(name, 0775);
    strcat(name, t_name);
    strcat(name, "/");
    if (stat(name, &st) == -1) mkdir(name, 0775);
    strcat(name, "met");
    
    fp = fopen(name, perm);
    if (!fp) printf("\nError in opening file\n");
    free(name);
    return fp;
}

FilePtr open_file_read(char* t_name , const char* perm){
    FilePtr fp;
    struct stat st = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    char *name = (char *)malloc(sizeof(char) * (2 * MAX_NAME + 15));
    strcpy(name, "./table/");
    strcat(name, t_name);
    strcat(name, "/met");

    if (stat(name, &st) == -1) {
        free(name);
        return NULL; 
    }

    fp = fopen(name, perm);
    free(name);
    return fp;
}
// --- UTILITY FUNCTIONS ---
int store_meta_data(struct table *t_ptr)
{
    // Fix: Explicitly pass the pointer to the first character to bypass array decay glitches
    FilePtr fp = open_file((t_ptr->name), "w"); 
    fwrite(t_ptr, sizeof(struct table), 1, fp);
    fclose(fp);
    return 0;
}

void store_meta_data_fstream(struct table *t_ptr) {
    // Fix: Explicit pointer passing here as well
    fstream_t fp = open_file_fstream((const char*)t_ptr->name, std::ios::out | std::ios::trunc | std::ios::binary);
    
    // Explicitly cast the struct to a char pointer for the write function
    fp.write(reinterpret_cast<char*>(t_ptr), sizeof(struct table));
    fp.close();
}

struct table* fetch_meta_data(string name){
    fstream_t fp = open_file_read_fstream(name.c_str(), READ_BIN); 
    
    if(!fp.is_open()) return NULL; 

    struct table* t = new table();
    fp.read(reinterpret_cast<char*>(t), sizeof(struct table));
    
    if(fp.fail() || fp.gcount() != (std::streamsize)sizeof(struct table)){
        cout << "ERROR! Failed to read metadata for table: " << name << endl;
        delete t;
        fp.close();
        return NULL;
    }
    fp.close();
    return t;
}