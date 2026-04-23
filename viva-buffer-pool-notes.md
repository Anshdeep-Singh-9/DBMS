# Viva Buffer Pool Notes

## Your Role

You built the storage and memory path of MiniDB:

- pack row values into bytes
- store bytes inside fixed-size pages
- track rows using `RID(page_id, slot_id)`
- build a Buffer Pool Manager over the disk layer
- connect insert and display to the new page-based flow

## Hemant's Role

Hemant strengthened the lower disk persistence layer:

- raw page open/read/write behavior
- stream flushing
- disk persistence testing

Simple relation:

- Hemant: page disk tak sahi ja raha hai ya nahi
- You: page RAM me kaise cache hoga, kab dirty hoga, kab flush hoga

## Big Picture

```text
User / CLI
-> Parser
-> Execution logic
-> B+ Tree or table scan
-> Buffer Pool Manager
-> Disk Manager
-> data.dat / index.dat on disk
```

## RID

`RID = (page_id, slot_id)`

Meaning:

- `page_id`: row kis page me hai
- `slot_id`: us page ke kis slot me hai

One-line explanation:

> RID is the logical address of a row inside page-based storage.

## Slotted Page

Page structure:

- page header
- slot directory
- free space
- tuple data

How it works:

- tuple bytes page ke back side se insert hote hain
- slot entries front side se grow karti hain
- free space beech me rehta hai

One-line explanation:

> A slotted page stores many rows in one fixed-size page while keeping each row addressable through slot entries.

## Tuple Serialization

Purpose:

- row values -> bytes
- bytes -> row values

Current support:

- `INT`
- `VARCHAR`

Encoding:

- `INT` = 4 bytes
- `VARCHAR` = 2-byte length + string bytes

One-line explanation:

> Tuple serialization is the bridge between logical row values and physical page bytes.

## DiskManager

Purpose:

- open/create page file
- allocate page
- read page
- write page
- compute offset using `page_id * page_size`

One-line explanation:

> DiskManager is the raw page I/O layer. It knows files and page offsets, not SQL or tuples.

## Hemant's Usefulness

Hemant's disk-layer improvements:

- added stronger flush behavior
- added low-level persistence testing
- proved allocate -> write -> close -> reopen -> read-back flow

Why it matters to you:

- buffer pool dirty pages eventually rely on DiskManager to persist changes
- without a stable disk layer, memory caching alone has no value

One-line explanation:

> My buffer pool depends on a correct disk persistence layer, so Hemant’s work directly supports my part.

## Buffer Pool Manager

Purpose:

- keep hot pages in RAM
- reduce repeated disk I/O
- manage page frames
- track dirty pages
- track pin count
- replace unpinned old pages using LRU

Important concepts:

- frame
- page table
- dirty bit
- pin count
- LRU replacement

APIs added:

- `fetch_page(page_id)`
- `new_page(page_id_out)`
- `unpin_page(page_id, is_dirty)`
- `flush_page(page_id)`
- `flush_all_pages()`

One-line explanation:

> BufferPoolManager keeps hot pages in RAM, tracks dirty and pinned pages, and reduces repeated disk I/O using LRU replacement.

## Insert Flow

Current flow:

1. read metadata
2. build schema
3. take row values
4. serialize tuple into bytes
5. check duplicate key in B+ Tree
6. fetch/create page through buffer pool
7. insert tuple into `DataPage`
8. get `slot_id`
9. create `RID(page_id, slot_id)`
10. unpin dirty page
11. flush page
12. update B+ Tree with `key -> RID`

One-line explanation:

> I moved insert from direct page I/O to buffer-pool-managed page updates.

## Display Flow

Current flow:

1. read metadata
2. build schema
3. fetch pages through buffer pool
4. load each page into `DataPage`
5. read tuple bytes by slot
6. deserialize tuple
7. print row values
8. unpin page as clean

One-line explanation:

> I completed the read path by making display fetch table pages through the buffer pool before decoding and printing tuples.

## B+ Tree Relation

B+ Tree role:

- fast primary key lookup
- stores `key -> RID`

Relation to your work:

- your storage layer defines what the `RID` means
- the index becomes meaningful because it points to page-based row addresses

One-line explanation:

> The B+ Tree gives fast key lookup, and my storage work gives the tree a meaningful physical row address through RID.

## What Is Tested

Working:

- build passes
- create table
- insert rows
- display rows
- read-back works correctly
- disk persistence test passes

Validated:

- rows inserted with `RID(0, 0)` and `RID(0, 1)`
- rows displayed back correctly
- raw page persistence test passes after reopen

## What Is Not Fully Done

Not fully complete:

- full crash recovery
- WAL
- redo/undo
- transactions
- full search-path migration
- full SQL execution migration
- joins
- backend API

Correct viva line:

> We support persistence and explicit flushing, but not full crash recovery. Proper crash recovery would require WAL and restart-time recovery logic.

## Persistence vs Crash Recovery

Persistence:

- data disk pe likha gaya
- reopen after normal close works

Crash recovery:

- sudden power failure ke baad database consistent restore ho
- needs WAL, redo/undo, checkpoints

Current status:

- persistence: yes
- crash recovery: no

## 1-Minute Viva Pitch

> My work was on the storage and memory architecture of MiniDB. I helped move the engine toward a proper page-based design using RID, slotted pages, and tuple serialization. Then I implemented a Buffer Pool Manager above the DiskManager so pages can be cached in RAM, tracked with dirty bits and pin counts, and managed with LRU replacement. Practically, I integrated this buffer pool into the insert and display paths, so now rows are written into pages, indexed using RID, fetched back through the buffer pool, deserialized, and displayed correctly.

## Hemant 20-Second Pitch

> Hemant worked on the lower disk persistence layer. His work improves and tests the raw DiskManager behavior, especially page allocation, write, close, reopen, and read-back correctness. My buffer pool depends on that lower layer, so his work is directly useful for making the full memory-to-disk path reliable.

## Common Viva Questions

### Why buffer pool if DiskManager already exists?

> DiskManager only knows how to read and write one page from the file. BufferPoolManager improves performance by caching frequently used pages in RAM, tracking dirty pages, and reducing repeated disk I/O.

### Why did you modify insert and display?

> Because those are the actual write and read paths. A buffer pool is only meaningful if real runtime operations use it. So I connected insert and display to the buffer pool instead of keeping them on direct disk access.

### What did you and Hemant achieve together?

> Together we completed the RAM-to-disk path in layers: Hemant strengthened the raw disk persistence layer, and I added the buffer pool and integrated it into insert and display. So now the system can cache pages in memory, flush them to disk, reopen the file later, and still read the stored data back correctly.
