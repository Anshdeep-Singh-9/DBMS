#include "../include/disk_manager.h"
#include <iostream>

#define BOLD "\033[1m"
#define GREEN "\033[32m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define RESET "\033[0m"

int main() {
    std::cout << BOLD << "\n=== Starting DiskManager Edge-Case Stress Test ===\n" << RESET;

    // 1. Setup
    std::string test_file = "test_stress.dat";
    DiskManager disk(test_file);
    if (!disk.open_or_create()) return 1;

    std::cout << "\n[1] Allocating 3 initial pages (Pages 0, 1, 2)...\n";
    disk.allocate_page(); // 0
    disk.allocate_page(); // 1
    disk.allocate_page(); // 2

    // 2. Attack 1: The Ghost Page
    std::cout << "\n[2] ATTACK: 'Ghost Page' Deallocation\n";
    std::cout << YELLOW << "    -> Maliciously telling the DiskManager to free Page 999...\n" << RESET;
    disk.deallocate_page(999);
    
    std::cout << "    -> Requesting a new page to see if it gives us 999...\n";
    uint32_t p3 = disk.allocate_page();
    if (p3 == 3) {
        std::cout << GREEN << "    [SUCCESS] Ghost page blocked! Safely expanded to Page 3.\n" << RESET;
    } else {
        std::cout << RED << "    [FAIL] Ghost page leaked into the vector!\n" << RESET;
    }

    // 3. Attack 2: The Double Free Corruption
    std::cout << "\n[3] ATTACK: 'Double Free' Corruption\n";
    std::cout << YELLOW << "    -> Deallocating Page 1...\n" << RESET;
    disk.deallocate_page(1);
    
    std::cout << YELLOW << "    -> Deallocating Page 1 AGAIN (Double Free)...\n" << RESET;
    disk.deallocate_page(1); 

    std::cout << "    -> Requesting two new pages...\n";
    uint32_t req1 = disk.allocate_page();
    uint32_t req2 = disk.allocate_page();

    std::cout << "    First request got Page: " << req1 << "\n";
    std::cout << "    Second request got Page: " << req2 << "\n";

    if (req1 == 1 && req2 == 4) {
        std::cout << GREEN << "    [SUCCESS] Double Free prevented! Page 1 was only recycled once. File safely expanded to Page 4.\n" << RESET;
    } else {
        std::cout << RED << "    [FAIL] Double Free occurred! Database corrupted.\n" << RESET;
    }

    // Cleanup
    disk.close();
    std::remove(test_file.c_str()); 
    std::cout << BOLD << "\n=== Test Complete ===\n\n" << RESET;

    return 0;
}