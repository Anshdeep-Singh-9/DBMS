#include "disk_manager.h"
#include <iostream>
#include <cassert>

void run_bitmap_test() {
    std::cout << "\n=== Starting Page 0 Bitmap Persistence Test ===\n";
    
    // --- Phase 1: Boot and Allocate ---
    std::cout << "[1] Creating brand new database file 'test_db.dat'...\n";
    DiskManager disk("test_db.dat");
    assert(disk.open_or_create() == true);
    
    // Allocate 3 pages. 
    // Because Page 0 is reserved for the Header, these should return 1, 2, and 3.
    uint32_t p1 = disk.allocate_page();
    uint32_t p2 = disk.allocate_page();
    uint32_t p3 = disk.allocate_page();
    std::cout << "    Allocated Pages: " << p1 << ", " << p2 << ", " << p3 << "\n";
    
    // --- Phase 2: Delete and Shutdown ---
    std::cout << "[2] User runs a DELETE query. Deallocating Page 2...\n";
    disk.deallocate_page(p2); // This flips Bit 2 to '0' inside Page 0
    
    std::cout << "[3] Shutting down the database server...\n";
    disk.close(); 
    // If we were still using the std::vector, the memory of Page 2 being empty would die right here.

    // --- Phase 3: Reboot and Verify ---
    std::cout << "[4] Next morning... Rebooting the database server...\n";
    DiskManager disk2("test_db.dat");
    assert(disk2.open_or_create() == true);
    
    std::cout << "[5] User runs an INSERT query. Requesting a new page...\n";
    uint32_t p4 = disk2.allocate_page(); // This should scan the Bitmap!
    
    std::cout << "    Database provided Page ID: " << p4 << "\n\n";
    
    // --- The Verdict ---
    if (p4 == 2) {
        std::cout << "✅ SUCCESS: The Page 0 Bitmap survived a reboot and correctly recycled Page 2!\n";
        std::cout << "   Your external fragmentation problem is officially solved.\n";
    } else {
        std::cout << "❌ FAIL: Expected Page 2, but got Page " << p4 << ". The Bitmap is not saving correctly.\n";
    }
    
    disk2.close();
    std::cout << "===============================================\n\n";
}

int main() {
    run_bitmap_test();
    return 0;
}