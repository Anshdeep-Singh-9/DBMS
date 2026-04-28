#include "buffer_pool_manager.h"

#include <algorithm>
#include <cstring>

namespace {
const uint32_t INVALID_FRAME_ID = UINT32_MAX;
}

/*
 * What:
 * This file implements the first practical Buffer Pool Manager for MiniDB.
 *
 * Why:
 * The project already has:
 * - page-based disk I/O through DiskManager
 * - slotted page storage through DataPage
 *
 * The next natural DBMS layer is memory caching. Without it, every page
 * request goes straight to disk, even if that page was used just one moment
 * earlier.
 *
 * Understanding:
 * A buffer pool is just an array of memory frames. Each frame can temporarily
 * hold one page from disk. A page can move like this:
 *
 * Disk page -> loaded into a frame -> used in RAM -> marked dirty if changed
 * -> written back later when flushed or evicted
 *
 * Concept used:
 * 1. Page table:
 *    maps page_id to frame_id
 * 2. Pin count:
 *    if pin_count > 0, do not evict that frame
 * 3. Dirty bit:
 *    if true, write frame back to disk before replacing it
 * 4. LRU:
 *    among unpinned frames, replace the least recently used one first
 *
 * Layman version:
 * - this is the RAM waiting room for disk pages
 * - hot pages stay in memory
 * - cold pages get kicked out later if memory becomes full
 */

BufferFrame::BufferFrame()
    : frame_id(INVALID_FRAME_ID),
      page_id(INVALID_PAGE_ID),
      is_dirty(false),
      pin_count(0),
      is_valid(false),
      page_data(STORAGE_PAGE_SIZE, 0) {
}

BufferFrame::BufferFrame(uint32_t id)
    : frame_id(id),
      page_id(INVALID_PAGE_ID),
      is_dirty(false),
      pin_count(0),
      is_valid(false),
      page_data(STORAGE_PAGE_SIZE, 0) {
}

BufferPoolManager::BufferPoolManager(std::size_t pool_size,
                                     DiskManager* disk_manager)
    : pool_size_(pool_size),
      disk_manager_(disk_manager),
      frames_(),
      page_table_(),
      lru_list_(),
      stats_() {
    frames_.reserve(pool_size_);
    for (std::size_t i = 0; i < pool_size_; ++i) {
        frames_.push_back(BufferFrame(static_cast<uint32_t>(i)));
    }
}

BufferPoolManager::~BufferPoolManager() {
    flush_all_pages();
}

char* BufferPoolManager::fetch_page(uint32_t page_id) {
    if (disk_manager_ == NULL || page_id == INVALID_PAGE_ID) {
        return NULL;
    }

    std::unordered_map<uint32_t, uint32_t>::iterator hit =
        page_table_.find(page_id);

    /*
     * Cache hit:
     * The page is already in RAM, so reuse that frame instead of re-reading
     * from disk.
     */
    if (hit != page_table_.end()) {
        BufferFrame& frame = frames_[hit->second];
        frame.pin_count++;
        stats_.cache_hits++;
        touch_lru(frame.frame_id);
        return &frame.page_data[0];
    }

    /*
     * Cache miss:
     * We need one frame to hold the requested page. Prefer a free frame first.
     * If none is free, choose an unpinned victim using LRU.
     */
    stats_.cache_misses++;
    uint32_t frame_id = has_free_frame() ? find_free_frame() : pick_victim_frame();
    if (frame_id == INVALID_FRAME_ID) {
        return NULL;
    }

    if (!load_page_into_frame(page_id, frame_id)) {
        return NULL;
    }

    BufferFrame& frame = frames_[frame_id];
    frame.pin_count = 1;
    touch_lru(frame.frame_id);
    return &frame.page_data[0];
}

char* BufferPoolManager::new_page(uint32_t& page_id_out) {
    page_id_out = INVALID_PAGE_ID;

    if (disk_manager_ == NULL) {
        return NULL;
    }

    uint32_t frame_id = has_free_frame() ? find_free_frame() : pick_victim_frame();
    if (frame_id == INVALID_FRAME_ID) {
        return NULL;
    }

    page_id_out = disk_manager_->allocate_page();
    if (page_id_out == INVALID_PAGE_ID) {
        return NULL;
    }

    BufferFrame& frame = frames_[frame_id];

    if (frame.is_valid) {
        if (!flush_frame(frame)) {
            return NULL;
        }
        page_table_.erase(frame.page_id);
    }

    frame.page_id = page_id_out;
    frame.is_valid = true;
    frame.is_dirty = true;
    frame.pin_count = 1;
    std::fill(frame.page_data.begin(), frame.page_data.end(), 0);

    page_table_[page_id_out] = frame_id;
    touch_lru(frame.frame_id);
    return &frame.page_data[0];
}

bool BufferPoolManager::unpin_page(uint32_t page_id, bool is_dirty) {
    std::unordered_map<uint32_t, uint32_t>::iterator found =
        page_table_.find(page_id);
    if (found == page_table_.end()) {
        return false;
    }

    BufferFrame& frame = frames_[found->second];
    if (frame.pin_count == 0) {
        return false;
    }

    frame.pin_count--;
    if (is_dirty) {
        frame.is_dirty = true;
    }

    /*
     * Once a page becomes unpinned, it may be replaced later if memory gets
     * full. We still keep it in RAM for fast reuse.
     */
    touch_lru(frame.frame_id);

    if (should_run_background_flush()) {
        checkpoint(1);
    }
    return true;
}

bool BufferPoolManager::flush_page(uint32_t page_id) {
    std::unordered_map<uint32_t, uint32_t>::iterator found =
        page_table_.find(page_id);
    if (found == page_table_.end()) {
        return false;
    }

    return flush_frame(frames_[found->second]);
}

void BufferPoolManager::flush_all_pages() {
    for (std::size_t i = 0; i < frames_.size(); ++i) {
        if (frames_[i].is_valid) {
            flush_frame(frames_[i]);
        }
    }
}

std::size_t BufferPoolManager::pool_size() const {
    return pool_size_;
}

std::size_t BufferPoolManager::cached_page_count() const {
    return page_table_.size();
}

std::size_t BufferPoolManager::pinned_page_count() const {
    std::size_t count = 0;
    for (std::size_t i = 0; i < frames_.size(); ++i) {
        if (frames_[i].is_valid && frames_[i].pin_count > 0) {
            count++;
        }
    }
    return count;
}

std::size_t BufferPoolManager::dirty_page_count() const {
    std::size_t count = 0;
    for (std::size_t i = 0; i < frames_.size(); ++i) {
        if (frames_[i].is_valid && frames_[i].is_dirty) {
            count++;
        }
    }
    return count;
}

const BufferPoolStats& BufferPoolManager::stats() const {
    return stats_;
}

std::size_t BufferPoolManager::checkpoint(std::size_t max_pages) {
    std::size_t flushed = 0;
    for (std::list<uint32_t>::iterator it = lru_list_.begin();
         it != lru_list_.end();
         ++it) {
        BufferFrame& frame = frames_[*it];
        if (!frame.is_valid || frame.pin_count > 0 || !frame.is_dirty) {
            continue;
        }

        if (!flush_frame(frame)) {
            continue;
        }

        flushed++;
        if (max_pages != 0 && flushed >= max_pages) {
            break;
        }
    }

    if (flushed > 0) {
        stats_.checkpoints++;
    }

    return flushed;
}

bool BufferPoolManager::has_free_frame() const {
    for (std::size_t i = 0; i < frames_.size(); ++i) {
        if (!frames_[i].is_valid) {
            return true;
        }
    }
    return false;
}

uint32_t BufferPoolManager::find_free_frame() const {
    for (std::size_t i = 0; i < frames_.size(); ++i) {
        if (!frames_[i].is_valid) {
            return static_cast<uint32_t>(i);
        }
    }
    return INVALID_FRAME_ID;
}

uint32_t BufferPoolManager::pick_victim_frame() {
    /*
     * LRU victim selection:
     * iterate from the least recently used side and find the first frame whose
     * pin_count is zero. Pinned frames are busy and must not be replaced.
     */
    for (std::list<uint32_t>::iterator it = lru_list_.begin();
         it != lru_list_.end();
         ++it) {
        BufferFrame& candidate = frames_[*it];
        if (candidate.pin_count == 0) {
            if (candidate.is_valid) {
                if (candidate.is_dirty) {
                    stats_.dirty_evictions++;
                } else {
                    stats_.clean_evictions++;
                }
            }
            if (!flush_frame(candidate)) {
                return INVALID_FRAME_ID;
            }
            page_table_.erase(candidate.page_id);
            candidate.page_id = INVALID_PAGE_ID;
            candidate.is_valid = false;
            candidate.is_dirty = false;
            candidate.pin_count = 0;
            std::fill(candidate.page_data.begin(), candidate.page_data.end(), 0);
            lru_list_.erase(it);
            return candidate.frame_id;
        }
    }

    return INVALID_FRAME_ID;
}

void BufferPoolManager::touch_lru(uint32_t frame_id) {
    lru_list_.remove(frame_id);
    lru_list_.push_back(frame_id);
}

bool BufferPoolManager::load_page_into_frame(uint32_t page_id, uint32_t frame_id) {
    if (page_id >= disk_manager_->page_count()) {
        return false;
    }

    BufferFrame& frame = frames_[frame_id];

    if (frame.is_valid) {
        if (!flush_frame(frame)) {
            return false;
        }
        page_table_.erase(frame.page_id);
    }

    if (!disk_manager_->read_page(page_id, &frame.page_data[0])) {
        return false;
    }

    frame.page_id = page_id;
    frame.is_valid = true;
    frame.is_dirty = false;
    frame.pin_count = 0;
    page_table_[page_id] = frame_id;
    return true;
}

bool BufferPoolManager::flush_frame(BufferFrame& frame) {
    if (!frame.is_valid) {
        return true;
    }

    if (!frame.is_dirty) {
        return true;
    }

    /*
     * Dirty frame means RAM has newer data than the file.
     * So before throwing the frame away, copy it back to disk.
     */
    if (!disk_manager_->write_page(frame.page_id, &frame.page_data[0])) {
        return false;
    }

    frame.is_dirty = false;
    stats_.dirty_flushes++;
    return true;
}

bool BufferPoolManager::should_run_background_flush() const {
    if (pool_size_ == 0) {
        return false;
    }

    /*
     * What:
     * Trigger a small checkpoint when too many dirty pages accumulate.
     *
     * Why:
     * A more complete buffer pool should not let all dirty pages pile up until
     * the very end. This keeps eviction and shutdown pressure lower.
     *
     * Understanding:
     * - if more than half the pool is dirty, start flushing old unpinned pages
     */
    return dirty_page_count() > (pool_size_ / 2);
}
