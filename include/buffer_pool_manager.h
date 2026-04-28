#ifndef BUFFER_POOL_MANAGER_H
#define BUFFER_POOL_MANAGER_H

#include <cstddef>
#include <cstdint>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#include "disk_manager.h"
#include "storage_types.h"

/*
 * What:
 * BufferPoolManager is the RAM-side caching layer placed above DiskManager.
 *  
 * Why:
 * Right now, if the engine wants the same page many times, it would have to
 * ask DiskManager to read that page from the file again and again. That is
 * correct logically, but wasteful in a DBMS. A buffer pool keeps frequently
 * used pages in memory so repeated access becomes faster.
 *
 * Understanding:
 * - DiskManager knows "how to read/write one page on disk".
 * - BufferPoolManager knows "which disk pages are currently kept in RAM".
 * - If a page is already in RAM, we reuse it.
 * - If a page is not in RAM, we load it from disk into an available frame.
 * - If RAM is full, we evict one old unpinned page using LRU.
 *
 * Concept used:
 * - Fixed-size page frames in memory
 * - Page table: page_id -> frame_id
 * - Pin count: how many users are actively holding a page
 * - Dirty bit: page changed in RAM and must be written back before eviction
 * - LRU policy: least recently used unpinned frame gets replaced first
 *
 * Layman version:
 * - DiskManager is the bookshelf.
 * - BufferPoolManager is the study table.
 * - Frequently needed pages are kept on the table instead of going back to
 *   the bookshelf every single time.
 */

struct BufferFrame {
    uint32_t frame_id;
    uint32_t page_id;
    bool is_dirty;
    uint32_t pin_count;
    bool is_valid;
    std::vector<char> page_data;

    BufferFrame();
    explicit BufferFrame(uint32_t id);
};

struct BufferPoolStats {
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t dirty_flushes;
    uint64_t clean_evictions;
    uint64_t dirty_evictions;
    uint64_t checkpoints;

    BufferPoolStats()
        : cache_hits(0),
          cache_misses(0),
          dirty_flushes(0),
          clean_evictions(0),
          dirty_evictions(0),
          checkpoints(0) {
    }
};

class BufferPoolManager {
  public:
    BufferPoolManager(std::size_t pool_size, DiskManager* disk_manager);
    ~BufferPoolManager();

    /*
     * What:
     * Fetch an existing page into the buffer pool and return a writable pointer
     * to its in-memory bytes.
     *
     * Why:
     * This is the main entry point for upper layers. Instead of directly asking
     * DiskManager every time, they ask the buffer pool first.
     *
     * Understanding:
     * - If the page is already cached -> cache hit
     * - Else -> cache miss, load from disk
     * - The frame pin count is increased so it is not evicted while in use
     */
    char* fetch_page(uint32_t page_id);

    /*
     * What:
     * Allocate a brand new disk page and place it into a buffer frame.
     *
     * Why:
     * Insert paths often need a fresh page instead of reading an existing one.
     */
    char* new_page(uint32_t& page_id_out);

    /*
     * What:
     * Release a page after use.
     *
     * Why:
     * A pinned page cannot be evicted. If upper layers never unpin pages, the
     * buffer pool will eventually get stuck with no replaceable frames.
     *
     * Understanding:
     * - is_dirty = true means RAM copy is newer than disk copy
     * - once pin count becomes zero, the frame becomes eligible for LRU
     */
    bool unpin_page(uint32_t page_id, bool is_dirty);

    /*
     * What:
     * Write one dirty page back to disk immediately.
     *
     * Why:
     * Needed for explicit control and safe eviction.
     */
    bool flush_page(uint32_t page_id);

    /*
     * What:
     * Write all dirty pages back to disk.
     *
     * Why:
     * Useful before shutdown or for demonstration/testing.
     */
    void flush_all_pages();

    std::size_t pool_size() const;
    std::size_t cached_page_count() const;
    std::size_t pinned_page_count() const;
    std::size_t dirty_page_count() const;
    const BufferPoolStats& stats() const;

    /*
     * What:
     * Flush a bounded number of dirty unpinned pages.
     *
     * Why:
     * A complete buffer manager should not wait only for shutdown or eviction.
     * It should be able to proactively reduce dirty pressure.
     *
     * Understanding:
     * - max_pages == 0 means "flush all eligible dirty pages"
     * - pinned pages are skipped because they are still in active use
     */
    std::size_t checkpoint(std::size_t max_pages = 0);

  private:
    std::size_t pool_size_;
    DiskManager* disk_manager_;
    std::vector<BufferFrame> frames_;
    std::unordered_map<uint32_t, uint32_t> page_table_;
    std::list<uint32_t> lru_list_;
    BufferPoolStats stats_;

    bool has_free_frame() const;
    uint32_t find_free_frame() const;
    uint32_t pick_victim_frame();
    void touch_lru(uint32_t frame_id);
    bool load_page_into_frame(uint32_t page_id, uint32_t frame_id);
    bool flush_frame(BufferFrame& frame);
    bool should_run_background_flush() const;
};

#endif
