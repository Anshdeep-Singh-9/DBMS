#include "declaration.h"
#include "BPtree.h"
#include "create.h"
#include "insert.h"
#include "display.h"
#include "search.h"
#include "drop.h"
#include "parser.h"
#include "file_handler.h"

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <filesystem>
#include <fstream>

using namespace std;

namespace fs = std::filesystem;

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

void pause_screen() {
    cout << DIM << "\nPress ENTER to continue..." << RESET;
    cin.ignore(10000, '\n');
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

    cout << "1. Show all tables in database\n";
    cout << "2. Create table using CREATE TABLE query\n";
    cout << "3. Insert data into an existing table\n";
    cout << "4. Drop table\n";
    cout << "5. Display table contents using SELECT query\n";
    cout << "6. Search table or search inside table\n";
    cout << "7. Print metadata of a table\n";
    cout << "8. Help\n";
    cout << "9. Quit\n\n";

    cout << BOLD << "Example CREATE query:\n" << RESET;
    cout << GREEN << "CREATE TABLE students (id INT, name VARCHAR(50));\n\n" << RESET;

    cout << BOLD << "Example SELECT query:\n" << RESET;
    cout << GREEN << "SELECT * FROM students;\n" << RESET;
    cout << GREEN << "SELECT name FROM students;\n" << RESET;

    print_small_line();
}

int take_input_option() {
    string option;

    print_line();
    cout << BOLD << WHITE << " MAIN MENU\n" << RESET;
    print_small_line();

    cout << CYAN << " 1 " << RESET << "Show all tables in database\n";
    cout << CYAN << " 2 " << RESET << "Create table\n";
    cout << CYAN << " 3 " << RESET << "Insert into table\n";
    cout << CYAN << " 4 " << RESET << "Drop table\n";
    cout << CYAN << " 5 " << RESET << "Display table contents\n";
    cout << CYAN << " 6 " << RESET << "Search table / search inside table\n";
    cout << CYAN << " 7 " << RESET << "Print metadata of a table\n";
    cout << CYAN << " 8 " << RESET << "Help\n";
    cout << CYAN << " 9 " << RESET << "Quit\n";

    print_line();
    cout << BOLD << "Enter choice [1-9]: " << RESET;

    cin >> option;

    if (option.length() != 1 || option[0] < '1' || option[0] > '9') {
        cout << RED << "\nInvalid input. Please enter a number from 1 to 9.\n" << RESET;
        return -1;
    }

    return option[0] - '0';
}

void input() {
    while (true) {
        int c = take_input_option();

        if (c == -1) {
            cin.clear();
            cin.ignore(10000, '\n');
            pause_screen();
            clear_screen();
            print_banner();
            continue;
        }

        switch (c) {
            case 1:
                print_section("Tables in Database");
                show_tables();
                pause_screen();
                break;

            case 2:
                print_section("Create Table");
                cout << YELLOW << "Enter CREATE TABLE query.\n" << RESET;
                cout << DIM << "Example: CREATE TABLE students (id INT, name VARCHAR(50));\n\n" << RESET;
                get_query();
                pause_screen();
                break;

            case 3:
                print_section("Insert Into Table");
                insert();
                pause_screen();
                break;

            case 4:
                print_section("Drop Table");
                drop();
                pause_screen();
                break;

            case 5:
                print_section("Display Table Contents");
                cout << YELLOW << "Enter SELECT query.\n" << RESET;
                cout << DIM << "Example: SELECT * FROM students;\n\n" << RESET;
                get_query();
                pause_screen();
                break;

            case 6:
                print_section("Search");
                cout << YELLOW << "Work in progress\n" << RESET;
                pause_screen();
                break;

            case 7:
                print_section("Table Metadata");
                display_meta_data();
                pause_screen();
                break;

            case 8:
                help();
                pause_screen();
                break;

            case 9:
                print_section("Exit");
                cout << GREEN << "MiniDB closed successfully.\n" << RESET;
                cout << BOLD << CYAN << "\nGood bye!\n\n" << RESET;
                exit(0);

            default:
                cout << RED << "\nPlease choose a correct option.\n" << RESET;
                pause_screen();
                break;
        }

        clear_screen();
        print_banner();
    }
}


void start_system() {
    system_check();
    clear_screen();
    print_banner();
    cout << GREEN << "Welcome to MiniDB Monitor.\n" << RESET;
    cout << DIM << "Use the numbered menu to perform DBMS operations.\n\n" << RESET;
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
                pause_screen();
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