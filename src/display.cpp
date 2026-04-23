#include "display.h"
#include "file_handler.h"
#include "where.h"
#include "disk_manager.h"
#include "data_page.h"
#include "tuple_serializer.h"
#include <iostream>
#include <iomanip>

// Internal helper for search_table
int search_table(char tab_name[]){
    char str[MAX_NAME+1];
    strcpy(str,"grep -Fxq ");
    strcat(str,tab_name);
    strcat(str," ./table/table_list");
    int x = system(str);
    if(x == 0) return 1;
    else return 0;
}

void display() {
    char tab[MAX_NAME];
    std::cout << "enter table name: ";
    std::cin >> tab;
    
    if(search_table(tab) == 0){
        printf("\nTable \" %s \" don't exist\n", tab);
        return ;
    }

    // Load Table Metadata
    struct table* meta = fetch_meta_data(tab);
    if (!meta) return;

    std::cout << "\nDisplaying contents for table: " << tab << "\n";
    std::cout << "--------------------------------------------------------------------------------\n";
    for(int i = 0; i < meta->count; i++) {
        std::cout << std::left << std::setw(20) << meta->col[i].col_name;
    }
    std::cout << "\n--------------------------------------------------------------------------------\n";

    // Prepare Schema for Deserialization
    std::vector<ColumnSchema> schema;
    for(int i = 0; i < meta->count; i++) {
        ColumnSchema col_schema(meta->col[i].col_name, 
                                meta->col[i].type == INT ? STORAGE_COLUMN_INT : STORAGE_COLUMN_VARCHAR,
                                meta->col[i].size);
        schema.push_back(col_schema);
    }

    // Open Data Storage
    std::string data_path = "table/";
    data_path += tab;
    data_path += "/data.dat";
    
    DiskManager data_disk(data_path);
    if (!data_disk.open_or_create()) {
        std::cout << "Error: Could not open data file." << std::endl;
        delete meta;
        return;
    }

    int row_count = 0;
    // Iterate through all pages
    for (uint32_t i = 0; i < data_disk.page_count(); ++i) {
        char buffer[STORAGE_PAGE_SIZE];
        if (!data_disk.read_page(i, buffer)) continue;

        DataPage page;
        page.load_from_buffer(buffer, STORAGE_PAGE_SIZE);

        // Iterate through all slots in the page
        for (uint16_t slot_id = 0; slot_id < page.slot_count(); ++slot_id) {
            std::vector<char> tuple_data;
            if (page.read_tuple(slot_id, tuple_data)) {
                std::vector<TupleValue> values;
                if (TupleSerializer::deserialize(schema, tuple_data, values)) {
                    for (const auto& val : values) {
                        if (val.type == STORAGE_COLUMN_INT) {
                            std::cout << std::left << std::setw(20) << val.int_value;
                        } else {
                            std::cout << std::left << std::setw(20) << val.string_value;
                        }
                    }
                    std::cout << "\n";
                    row_count++;
                }
            }
        }
    }

    if (row_count == 0) {
        std::cout << "(Table is empty)\n";
    }

    std::cout << "--------------------------------------------------------------------------------\n";
    std::cout << "Total rows: " << row_count << "\n\n";
    delete meta;
}

void show_tables(){
	char name[MAX_NAME];
	printf("\n\nLIST OF TABLES\n\n");
	printf("---------------------------\n");
	int tabcount = 0;
	FilePtr fp = fopen("./table/table_list","r");
    if (fp) {
	    while(fscanf(fp,"%s",name) != EOF){
		    tabcount++;
		    std::cout << name << "\n";
	    }
        fclose(fp);
    }
	if(tabcount == 0) printf("Database empty!!!\n");
	printf("---------------------------\n");
}

void display_meta_data(){
	std::string name;
	std::cout<<"Enter the name of table: ";
	std::cin>>name;
	
	if(name.empty()){
		std::cout<<"ERROR! No name entered. Exiting!!!\n";
		return;
	}

	struct table* t_ptr = fetch_meta_data(name);
	if(t_ptr == NULL){
		std::cout<<"ERROR! Table not found or failed to read metadata.\n";
		return;
	}
    std::cout<<"Table Name: "<<t_ptr->name<<std::endl<<std::endl;
	std::cout<<"Max Row Size: "<<t_ptr->size<<std::endl<<std::endl;
    std::cout<<"Table Attributes:\n";
    for(int i=0; i<t_ptr->count; i++){
        std::cout<<"name: "<<t_ptr->col[i].col_name<<std::endl;
        std::cout<<"type: "<<((t_ptr->col[i].type == 1) ? "INT" : "VARCHAR")<<std::endl;
        std::cout<<"size: "<<t_ptr->col[i].size<<std::endl<<std::endl;
    }
    delete t_ptr;
}

// Keeping the old complex functions for completeness, but they might need separate refactoring
// to be fully compatible with the new page architecture for complex queries.
void display(char tab[], std::map<std::string, int> &display_col_list);

// --- NEW SELECT IMPLEMENTATION ---

// Helper function to execute the query using the New Page Architecture
void execute_select(const std::string& tab_name, const std::vector<std::string>& target_cols) {
    char tab[MAX_NAME];
    strcpy(tab, tab_name.c_str());

    if(search_table(tab) == 0){
        std::cout << "\nTable \"" << tab << "\" doesn't exist\n";
        return;
    }

    struct table* meta = fetch_meta_data(tab);
    if (!meta) return;

    // 1. Determine which columns the user wants to see
    bool select_all = (target_cols.size() == 1 && target_cols[0] == "*");
    std::vector<int> col_indices_to_print; 

    for(const auto& t_col : target_cols) {
        if (select_all) break;
        bool found = false;
        for(int i = 0; i < meta->count; i++) {
            if (t_col == meta->col[i].col_name) {
                col_indices_to_print.push_back(i); // Save the index of the requested column
                found = true;
                break;
            }
        }
        if (!found) {
            std::cout << "Error: Column '" << t_col << "' does not exist in table.\n";
            delete meta;
            return;
        }
    }

    // If "*", add all column indices
    if (select_all) {
        for(int i = 0; i < meta->count; i++) col_indices_to_print.push_back(i);
    }

    // 2. Print Header
    std::cout << "\n--------------------------------------------------------------------------------\n";
    for(int idx : col_indices_to_print) {
        std::cout << std::left << std::setw(20) << meta->col[idx].col_name;
    }
    std::cout << "\n--------------------------------------------------------------------------------\n";

    // 3. Prepare Full Schema (We must deserialize the whole row, even if we only print a few columns)
    std::vector<ColumnSchema> schema;
    for(int i = 0; i < meta->count; i++) {
        ColumnSchema col_schema(meta->col[i].col_name, 
                                meta->col[i].type == INT ? STORAGE_COLUMN_INT : STORAGE_COLUMN_VARCHAR,
                                meta->col[i].size);
        schema.push_back(col_schema);
    }

    // 4. Open Data Storage (New Architecture)
    std::string data_path = "table/" + tab_name + "/data.dat";
    DiskManager data_disk(data_path);
    if (!data_disk.open_or_create()) {
        std::cout << "Error: Could not open data file.\n";
        delete meta;
        return;
    }

    int row_count = 0;
    // 5. Read Pages and Slots
    for (uint32_t i = 0; i < data_disk.page_count(); ++i) {
        char buffer[STORAGE_PAGE_SIZE];
        if (!data_disk.read_page(i, buffer)) continue;

        DataPage page;
        page.load_from_buffer(buffer, STORAGE_PAGE_SIZE);

        for (uint16_t slot_id = 0; slot_id < page.slot_count(); ++slot_id) {
            std::vector<char> tuple_data;
            if (page.read_tuple(slot_id, tuple_data)) {
                std::vector<TupleValue> values;
                if (TupleSerializer::deserialize(schema, tuple_data, values)) {
                    
                    // ONLY print the requested columns using our saved indices
                    for (int idx : col_indices_to_print) {
                        if (values[idx].type == STORAGE_COLUMN_INT) {
                            std::cout << std::left << std::setw(20) << values[idx].int_value;
                        } else {
                            std::cout << std::left << std::setw(20) << values[idx].string_value;
                        }
                    }
                    std::cout << "\n";
                    row_count++;
                }
            }
        }
    }

    if (row_count == 0) std::cout << "(Table is empty)\n";
    std::cout << "--------------------------------------------------------------------------------\n";
    std::cout << "Total rows: " << row_count << "\n\n";
    delete meta;
}

// The Parser Integration Hook
void process_select(std::vector <std::string> &token_vector){
    if (token_vector.size() < 4 || token_vector[0] != "select") {
        std::cout << "Syntax Error: Invalid SELECT statement.\n";
        return;
    }

    std::vector<std::string> target_cols;
    std::string table_name = "";
    int from_index = -1;

    // 1. Find 'from' to separate columns from table name
    for (size_t i = 1; i < token_vector.size(); i++) {
        if (token_vector[i] == "from") {
            from_index = i;
            break;
        } else {
            target_cols.push_back(token_vector[i]);
        }
    }

    // Validation
    if (from_index == -1 || from_index + 1 >= token_vector.size()) {
        std::cout << "Syntax Error: Missing FROM clause or table name.\n";
        return;
    }

    // 2. Extract Table Name
    table_name = token_vector[from_index + 1];

    // [FUTURE HOOK]: Check for WHERE clause right here!
    // if (token_vector.size() > from_index + 2 && token_vector[from_index + 2] == "where") { ... }

    // 3. Execute!
    execute_select(table_name, target_cols);
}