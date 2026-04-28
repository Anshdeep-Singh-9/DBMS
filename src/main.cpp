#include "declaration.h"
#include "display.h"
#include "parser.h"
#include "file_handler.h"
#include "recovery_manager.h"
#include "where.h"

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <limits>
#include <cctype>

using namespace std;

void system_check();
void execute_query_string(string input_query);

#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define DIM     "\033[2m"
#define CYAN    "\033[36m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define RED     "\033[31m"
#define WHITE   "\033[97m"

void clear_screen() {
    system("clear");
}

void print_line() {
    cout << CYAN << "======================================================================" << RESET << "\n";
}

void print_small_line() {
    cout << DIM << "----------------------------------------------------------------------" << RESET << "\n";
}

void pause_screen(bool clearLeftoverNewline = false) {
    cout << DIM << "\nPress ENTER to continue..." << RESET;

    cin.clear();

    if (clearLeftoverNewline) {
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    cin.get();
}

void print_banner() {
    print_line();

    cout << BOLD << CYAN;
    cout << "  __  __ _       _ ____  ____  \n";
    cout << " |  \\/  (_)_ __ (_)  _ \\| __ ) \n";
    cout << " | |\\/| | | '_ \\| | | | |  _ \\ \n";
    cout << " | |  | | | | | | | |_| | |_) |\n";
    cout << " |_|  |_|_|_| |_|_|____/|____/ \n";
    cout << RESET;

    cout << BOLD << WHITE << "\n        MiniDB Engine - Terminal DBMS Monitor\n" << RESET;
    cout << DIM << "        Storage Engine | B+ Tree Index | Buffer Pool | SQL Parser\n" << RESET;

    print_line();
}

void print_section(string title) {
    clear_screen();
    print_banner();

    cout << BOLD << WHITE << " " << title << RESET << "\n";
    print_small_line();
}

void help() {
    print_section("Help");

    cout << BOLD << "MiniDB Supported Operations\n" << RESET;
    print_small_line();

    cout << "1. Query Console\n";
    cout << "2. Search table or search inside table\n";
    cout << "3. Print metadata of a table\n";
    cout << "4. Help\n";
    cout << "5. Quit\n\n";

    cout << BOLD << "Supported Query Console Syntax\n" << RESET;
    print_small_line();

    cout << GREEN << "SHOW TABLES;\n" << RESET;
    cout << GREEN << "CREATE TABLE students (id INT, name VARCHAR(50), dept VARCHAR(20));\n" << RESET;
    cout << GREEN << "INSERT INTO students VALUES (1, \"Aditya\", \"CSE\");\n" << RESET;
    cout << GREEN << "SELECT * FROM students;\n" << RESET;
    cout << GREEN << "SELECT name, dept FROM students;\n" << RESET;
    cout << GREEN << "SELECT * FROM students WHERE id = 1;\n" << RESET;
    cout << GREEN << "UPDATE students SET dept = ECE WHERE id = 1;\n" << RESET;
    cout << GREEN << "DROP TABLE students;\n" << RESET;

    cout << "\n" << BOLD << "Notes\n" << RESET;
    print_small_line();
    cout << "- SQL keywords are case-insensitive.\n";
    cout << "- Table names and column names are case-sensitive.\n";
    cout << "- VARCHAR values keep the original case exactly as typed.\n";
    cout << "- Quotes are optional for VARCHAR values, but recommended for clarity.\n";
    cout << "- First column must be INT because it is used as primary key.\n";
    cout << "- INSERT values must follow the same order as table columns.\n";
    cout << "- UPDATE supports WHERE column = value.\n";
    cout << "- Primary key WHERE routes to B+ Tree lookup, non-primary WHERE uses linear scan.\n";
    cout << "- DROP TABLE is available through Query Console.\n";
    cout << "- Type BACK or EXIT inside Query Console to return to the main menu.\n";

    print_small_line();
}

int take_input_option() {
    string option;

    print_line();

    cout << BOLD << WHITE << " MAIN MENU\n" << RESET;
    print_small_line();

    cout << CYAN << " 1 " << RESET << "Query Console  (CREATE / INSERT / SELECT / SHOW / DROP)\n";
    cout << CYAN << " 2 " << RESET << "Search table / search inside table\n";
    cout << CYAN << " 3 " << RESET << "Print metadata of a table\n";
    cout << CYAN << " 4 " << RESET << "Help\n";
    cout << CYAN << " 5 " << RESET << "Quit\n";

    print_line();

    cout << BOLD << "Enter choice [1-5]: " << RESET;
    cin >> option;

    if (option.length() != 1 || option[0] < '1' || option[0] > '5') {
        cout << RED << "\nInvalid input. Please enter a number from 1 to 5.\n" << RESET;
        return -1;
    }

    return option[0] - '0';
}

string normalize_console_command(string s) {
    while (!s.empty() && isspace((unsigned char)s.front())) {
        s.erase(s.begin());
    }

    while (!s.empty() && isspace((unsigned char)s.back())) {
        s.pop_back();
    }

    if (!s.empty() && s.back() == ';') {
        s.pop_back();
    }

    for (char &c : s) {
        c = static_cast<char>(tolower((unsigned char)c));
    }

    return s;
}

void print_query_console_syntax() {
    cout << BOLD << "Syntax Guide\n" << RESET;
    print_small_line();

    cout << GREEN << "SHOW TABLES;\n" << RESET;
    cout << GREEN << "CREATE TABLE students (id INT, name VARCHAR(50), dept VARCHAR(20));\n" << RESET;
    cout << GREEN << "INSERT INTO students VALUES (1, \"Aditya\", \"CSE\");\n" << RESET;
    cout << GREEN << "SELECT * FROM students;\n" << RESET;
    cout << GREEN << "SELECT name, dept FROM students;\n" << RESET;
    cout << GREEN << "SELECT * FROM students WHERE id = 1;\n" << RESET;
    cout << GREEN << "UPDATE students SET dept = ECE WHERE id = 1;\n" << RESET;
    cout << GREEN << "DROP TABLE students;\n" << RESET;

    print_small_line();
}

void query_console_loop() {
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    while (true) {
        cout << "\n" << BOLD << "MiniDB Query Console" << RESET << "\n";
        print_small_line();

        cout << DIM << "Type BACK or EXIT to return to main menu.\n\n" << RESET;

        print_query_console_syntax();

        cout << BOLD << "\nminidb> " << RESET;

        string query;
        getline(cin, query);

        string command = normalize_console_command(query);

        if (command == "back" || command == "exit") {
            cout << GREEN << "Returning to main menu...\n" << RESET;
            break;
        }

        execute_query_string(query);
    }
}

void input() {
    while (true) {
        int c = take_input_option();

        if (c == -1) {
            cin.clear();
            pause_screen(true);
            clear_screen();
            print_banner();
            continue;
        }

        switch (c) {
            case 1:
                print_section("Query Console");
                query_console_loop();
                pause_screen(false);
                break;

            case 2: {
                print_section("Search Inside Table");

                string search_table_name;
                cout << BOLD << "Enter table name: " << RESET;
                cin >> search_table_name;

                char tab_check[MAX_NAME];
                strncpy(tab_check, search_table_name.c_str(), MAX_NAME - 1);
                tab_check[MAX_NAME - 1] = '\0';

                if (search_table(tab_check) == 0) {
                    cout << RED << "Error: Table '" << search_table_name
                         << "' does not exist." << RESET << "\n";
                    pause_screen(true);
                    break;
                }

                table* search_meta = fetch_meta_data(search_table_name);
                if (search_meta == NULL) {
                    cout << RED << "Error: Could not load metadata." << RESET << "\n";
                    pause_screen(true);
                    break;
                }

                cout << "\n" << BOLD << "Available columns:\n" << RESET;
                for (int ci = 0; ci < search_meta->count; ci++) {
                    string type_str = (search_meta->col[ci].type == INT) ? "INT" : "VARCHAR";
                    cout << "  [" << ci << "] "
                         << search_meta->col[ci].col_name
                         << " (" << type_str << ")";
                    if (ci == 0) cout << " <- Primary Key";
                    cout << "\n";
                }

                string search_col, search_val;
                cout << "\n" << BOLD << "Enter column to search on: " << RESET;
                cin >> search_col;
                cout << BOLD << "Enter value to search for: " << RESET;
                cin >> ws;
                getline(cin, search_val);

                if (search_val.size() >= 2 &&
                    ((search_val.front() == '"' && search_val.back() == '"') ||
                     (search_val.front() == '\'' && search_val.back() == '\''))) {
                    search_val = search_val.substr(1, search_val.size() - 2);
                }

                delete search_meta;

                WhereClause wc;
                wc.present = true;
                wc.column = search_col;
                wc.op = "=";
                wc.value = search_val;

                vector<string> all_cols;
                all_cols.push_back("*");
                execute_select_where(search_table_name, all_cols, wc);

                pause_screen(true);
                break;
            }

            case 3:
                print_section("Table Metadata");
                display_meta_data();
                pause_screen(true);
                break;

            case 4:
                help();
                pause_screen(true);
                break;

            case 5:
                print_section("Exit");
                cout << GREEN << "MiniDB closed successfully.\n" << RESET;
                cout << BOLD << CYAN << "\nGood bye!\n\n" << RESET;
                exit(0);

            default:
                cout << RED << "\nPlease choose a correct option.\n" << RESET;
                pause_screen(true);
                break;
        }

        clear_screen();
        print_banner();
    }
}

void start_system() {
    system_check();
    RecoveryManager::recover_all_tables();

    clear_screen();
    print_banner();

    cout << GREEN << "Welcome to MiniDB Monitor.\n" << RESET;
    cout << DIM << "Use Query Console to execute SQL-like commands.\n\n" << RESET;

    input();
}

string get_password() {
    struct termios termios_p;

    tcgetattr(STDIN_FILENO, &termios_p);

    termios_p.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &termios_p);

    cout << BOLD << "Enter Password: " << RESET;

    string pass;
    cin >> pass;

    termios_p.c_lflag |= ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &termios_p);

    cout << "\n";

    return pass;
}

int main(int argc, char *argv[]) {
    if (argc == 4 || argc == 5) {
        if (strcmp(argv[1], "-u") == 0 && strcmp(argv[3], "-p") == 0) {
            const string mypass = "pass";

            clear_screen();
            print_banner();

            cout << BOLD << "User: " << RESET << argv[2] << "\n\n";

            string password = get_password();

            if (password == mypass) {
                cout << GREEN << "Correct password!\n" << RESET;
            } else {
                cout << RED << "Incorrect password!\n" << RESET;
                return 0;
            }
        } else {
            cout << RED << "\nInvalid usage.\n" << RESET;
            cout << "Usage: ./minidb -u username -p\n";
            return 0;
        }
    } else {
        cout << RED << "\nInvalid usage.\n" << RESET;
        cout << "Usage: ./minidb -u username -p\n";
        return 0;
    }

    start_system();

    return 0;
}
