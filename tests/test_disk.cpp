#include <cstring>
#include <iostream>

#include "disk_manager.h"

/*
 * What:
 * This is Hemant's low-level persistence test adapted into the local repo
 * without placing it under src/, so it does not interfere with the main
 * miniDB executable build.
 *
 * Why:
 * We want to prove the raw DiskManager layer can:
 * - allocate pages
 * - write bytes to them
 * - close the file
 * - reopen the file
 * - read the same bytes back correctly
 *
 * Understanding:
 * This test validates the physical disk layer only. It does not test B+ Tree,
 * DataPage, or BufferPoolManager policies. It answers one focused question:
 *
 * "If we ask DiskManager to persist page bytes, do we get the same bytes back
 * after reopening the file?"
 *
 * Layman version:
 * - write two pages
 * - close the database file
 * - open it again
 * - check whether the written text is still there
 */

int main() {
    const std::string test_file = "test_data.dat";
    std::cout << "--- Starting Disk Manager Persistence Test ---\n";

    DiskManager disk(test_file, STORAGE_PAGE_SIZE);
    if (!disk.open_or_create()) {
        std::cout << "[FAILED] Could not open/create test file.\n";
        return 1;
    }

    std::cout << "[SUCCESS] File opened. Current pages: "
              << disk.page_count() << "\n";

    const uint32_t page_0 = disk.allocate_page();
    const uint32_t page_1 = disk.allocate_page();
    std::cout << "[SUCCESS] Allocated Page ID: " << page_0
              << " and Page ID: " << page_1 << "\n";

    char write_buffer_0[STORAGE_PAGE_SIZE] = {0};
    char write_buffer_1[STORAGE_PAGE_SIZE] = {0};

    std::strcpy(write_buffer_0, "Hello from Page 0!");
    std::strcpy(write_buffer_1, "Data safely stored in Page 1!");

    disk.write_page(page_0, write_buffer_0);
    disk.write_page(page_1, write_buffer_1);
    std::cout << "[SUCCESS] Wrote distinct data to both pages.\n";

    disk.close();
    std::cout << "[SUCCESS] DiskManager closed (simulating shutdown).\n\n";

    std::cout << "--- Rebooting Database ---\n";
    DiskManager disk_reboot(test_file, STORAGE_PAGE_SIZE);
    if (!disk_reboot.open_or_create()) {
        std::cout << "[FAILED] Could not reopen test file.\n";
        return 1;
    }

    std::cout << "[SUCCESS] File reopened. Found "
              << disk_reboot.page_count() << " pages on disk.\n";

    char read_buffer_0[STORAGE_PAGE_SIZE] = {0};
    char read_buffer_1[STORAGE_PAGE_SIZE] = {0};

    disk_reboot.read_page(page_0, read_buffer_0);
    disk_reboot.read_page(page_1, read_buffer_1);

    std::cout << "Reading Page 0: " << read_buffer_0 << "\n";
    std::cout << "Reading Page 1: " << read_buffer_1 << "\n";

    if (std::strcmp(read_buffer_0, "Hello from Page 0!") == 0 &&
        std::strcmp(read_buffer_1, "Data safely stored in Page 1!") == 0) {
        std::cout << "\n[TEST PASSED] Disk persistence is accurate.\n";
        return 0;
    }

    std::cout << "\n[TEST FAILED] Data mismatch.\n";
    return 1;
}
