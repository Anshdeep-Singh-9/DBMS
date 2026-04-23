# MiniDB Implementation Report

This report compares the **original proposed scope** of the project with the **current merged implementation on `main`** after pulling the latest changes.

It is written to help the team understand:

- what has actually been implemented
- what is partially implemented
- what still remains planned or incomplete
- which files currently own each subsystem

## 1. Executive Summary

The project is no longer only an old row-file prototype. It is now a **hybrid MiniDB codebase**:

- some important paths have already moved to the **new page-based storage model**
- some old paths are still present and still use the **legacy row-per-file logic**
- a few menu options and SQL-style paths are currently **work in progress**

### Current practical state

What works in the merged codebase today:

- CLI login and menu flow
- table listing
- table creation
- table drop
- row insertion into **page-based storage**
- table display from **page-based storage**
- metadata display
- persistent B+ Tree storage with page-backed index nodes
- tuple serialization and deserialization for `INT` and `VARCHAR`

What is partial or not fully integrated yet:

- SQL `SELECT` parsing exists, but full new-storage execution is not connected
- search option in the menu is marked work in progress
- old `WHERE` / search files still use legacy row files
- buffer pool manager is not implemented
- join support is not implemented
- backend API layer is not implemented
- frontend/UI is not integrated into the main runtime

## 2. Original Proposal vs Current Reality

## Original proposed goals

From the project proposal and README, the system aimed to support:

- table management
- file-based table storage
- metadata handling
- insert and search
- SQL-like query execution
- filtering with `WHERE`, `=`, `<`, `>`, `AND`, `OR`
- B+ Tree indexing
- display engine
- command-line interface
- buffer pool manager
- join support
- backend API
- optional React frontend

## Current merged reality

The codebase currently delivers the following:

| Area | Status | Notes |
| --- | --- | --- |
| CLI / menu | Implemented | `src/main.cpp` |
| Table management | Implemented | `create`, `drop`, `show tables` |
| Metadata handling | Implemented | `src/file_handler.cpp` |
| Insert | Implemented on new storage path | `src/insert.cpp` |
| Display all rows | Implemented on new storage path | `src/display.cpp` |
| B+ Tree indexing | Implemented, persistent | `src/BPtree.cpp` |
| Tuple serialization | Implemented | `src/tuple_serializer.cpp` |
| SQL tokenization / parsing | Partial | `src/parser.cpp` |
| `WHERE` filtering | Partial / legacy path | `src/where.cpp` |
| Search from menu | Not active | menu says work in progress |
| Buffer pool manager | Not implemented | only planned in docs |
| Join support | Not implemented | no join execution path |
| Backend API | Not implemented | not in runtime |
| Frontend | Not implemented in runtime | only mentioned in planning |

## 3. High-Level Architecture of the Current Codebase

The current codebase is best understood as these active layers:

1. **CLI layer**
   - menu input and command routing
2. **Metadata layer**
   - table schema read/write
3. **Storage foundation**
   - pages, page headers, slot directory, tuple bytes
4. **Tuple encoding layer**
   - convert row values to bytes and bytes back to row values
5. **Index layer**
   - persistent B+ Tree storing key to `RID`
6. **Legacy query layer**
   - old `search` and `where` logic still exists, mostly file-based

This means the project is currently in a **transition stage**:

- insert and display already use the new storage model
- search and SQL filtering are not fully migrated yet

## 4. File-by-File / Topic-by-Topic Status

## 4.1 Main Program / CLI

**Files**

- `src/main.cpp`

**What it does now**

- handles login with password check
- starts the menu-driven MiniDB interface
- supports:
  - show tables
  - create table
  - insert into table
  - drop table
  - display table contents
  - print metadata
  - help

**Important current limitation**

- option `6` for search is currently not wired to actual search logic
- it prints: `Work in progress`

**Status**

- Implemented, but one menu path is intentionally disabled

## 4.2 Table Creation and Metadata

**Files**

- `src/create.cpp`
- `src/file_handler.cpp`

**What it does now**

- creates table directory under `table/<table_name>/`
- stores metadata in `table/<table_name>/met`
- appends table name to `table/table_list`
- records column names, types, sizes, and record layout info

**What is good**

- creation and metadata storage are working
- metadata is persistent
- file handling has been cleaned up using a mix of `FILE*` and `std::filesystem` / fstream helpers

**What is still old**

- metadata format still uses the older `table` struct design
- table creation is not yet fully redesigned around a newer richer schema file format

**Status**

- Implemented and usable

## 4.3 Parser and Query Tokenization

**Files**

- `src/parser.cpp`

**What it does now**

- tokenizes `SELECT` queries
- respects double quotes while tokenizing string values
- avoids splitting spaces inside quoted strings
- has a very basic `CREATE` tokenization path

**What has improved**

- standard quoted string handling is better than before
- strings with spaces are now handled more safely during tokenization

**What is still missing**

- this is still not a full SQL parser / AST planner
- parser is only lightly connected to execution
- `process_select()` currently does not run a full new-storage select pipeline

**Status**

- Partial

## 4.4 Display / Output Engine

**Files**

- `src/display.cpp`
- `include/display.h`

**What it does now**

- shows all tables
- displays metadata
- displays full table contents using the **new page-based storage path**

**Current internal flow**

1. read table metadata
2. build storage schema from metadata
3. open `table/<table_name>/data.dat`
4. iterate through all pages
5. iterate through all slots in each page
6. read tuple bytes
7. deserialize tuple
8. print values column-by-column

**Important note**

- standard display from menu option `5` now uses the new storage format
- SQL-style `SELECT` processing is still not fully migrated

**Status**

- Implemented for standard table display
- Partial for SQL query display path

## 4.5 Insert Path

**Files**

- `src/insert.cpp`

**What it does now**

- validates table existence
- loads metadata
- collects user input for each column
- builds row values in memory
- serializes the tuple
- stores tuple into a slotted page in `data.dat`
- creates an `RID(page_id, slot_id)`
- inserts primary key into the B+ Tree

**Current internal flow**

1. load schema from metadata
2. build `ColumnSchema` list
3. build `TupleValue` list
4. serialize tuple into bytes
5. open data file using `DiskManager`
6. scan pages for free space
7. allocate new page if needed
8. insert tuple into `DataPage`
9. write page back
10. index the row using `RID`

**This is one of the biggest merged changes**

- insertion is no longer using old `fileN.dat` row-file logic
- insertion is now page-based

**Status**

- Implemented on the new architecture

## 4.6 Tuple Serialization

**Files**

- `include/tuple_serializer.h`
- `src/tuple_serializer.cpp`

**What it does now**

- converts logical row values into stored byte format
- converts stored tuple bytes back into row values

**Supported types**

- `INT`
- `VARCHAR`

**Encoding model**

- `INT` -> fixed 4 bytes
- `VARCHAR` -> 2-byte length + actual string bytes

**Why it matters**

- this is the bridge between row values and page storage
- `DataPage` stores bytes, so serializer is required for real record storage

**Status**

- Implemented

## 4.7 Page-Based Storage Foundation

**Files**

- `include/storage_types.h`
- `include/disk_manager.h`
- `src/disk_manager.cpp`
- `include/data_page.h`
- `src/data_page.cpp`

**What it does now**

- defines the page-based storage model
- provides fixed-size page handling
- provides page header and slot directory structure
- supports storing multiple tuples in one page

**Main concepts implemented**

- `RID(page_id, slot_id)`
- `PageHeader`
- `SlotEntry`
- `DiskManager`
- `DataPage`

**What each part does**

- `RID`
  - record address in page-based storage
- `DiskManager`
  - opens storage file
  - allocates pages
  - reads one page
  - writes one page
- `DataPage`
  - manages tuple placement within one page
  - inserts tuples from the back
  - tracks slots from the front
  - reads tuple bytes back using slot ids

**Status**

- Implemented

## 4.8 B+ Tree Index

**Files**

- `include/BPtree.h`
- `src/BPtree.cpp`

**What it does now**

- stores a persistent page-backed B+ Tree
- saves root metadata in page `0` of `table/<table_name>/index.dat`
- supports search
- supports insert
- supports leaf splitting and internal splitting
- uses `RID` as stored leaf value in the current main insert path

**What is strong now**

- index is no longer just an in-memory tree
- tree nodes are stored in a disk file
- insert path is aligned to `key -> RID`

**Important compatibility note**

- old helper methods such as `insert_record(int key, int record_num)` still exist
- this means some compatibility with old record-number style remains in the code

**Status**

- Implemented

## 4.9 Search

**Files**

- `src/search.cpp`

**What it does now**

- old search implementation still exists
- allows:
  - table existence search
  - primary key search using B+ Tree
- but the row fetch logic still opens legacy `fileN.dat`

**Important runtime note**

- current menu in `src/main.cpp` does not call this file anymore
- menu option `6` is marked work in progress

**Status**

- Legacy code exists
- Not currently active from main CLI
- Not migrated to new page-based row fetch

## 4.10 WHERE / Conditional Filtering

**Files**

- `src/where.cpp`

**What it does now**

- supports parts of old conditional selection logic
- checks whether selected columns exist
- uses B+ Tree for primary-key condition
- uses brute-force scan for non-primary-key condition
- enforces double quotes for string literal comparisons

**Important limitation**

- actual row reading still depends on `fileN.dat`
- this is still coupled to the legacy storage path

**Status**

- Partial
- Legacy execution path

## 4.11 Drop Table

**Files**

- `src/drop.cpp`

**What it likely does in project flow**

- remove table artifacts
- update `table/table_list`

**Status**

- Exposed in menu and treated as implemented

## 5. Feature Status Against Original Proposed Topics

## Table management

**Status:** Implemented

- create table works
- show tables works
- drop table is present in runtime

## File-based storage

**Status:** Implemented, but changed in design

- metadata still uses separate files/directories
- actual row insertion/display has now moved toward page-based storage in `data.dat`
- legacy row-file logic still exists in older paths

## Metadata handling

**Status:** Implemented

- table schema is stored and read successfully

## Insert

**Status:** Implemented

- now uses serializer + slotted page + `RID` + B+ Tree

## Search

**Status:** Partial

- B+ Tree search logic exists
- old search code exists
- main menu search path is not active
- full new-storage search path is not completed

## Query execution engine

**Status:** Partial

- parser exists
- some select tokenization exists
- full execution routing across the new storage path is not complete

## Filtering with operators and logical operators

**Status:** Partial / legacy

- old `WHERE` logic exists
- still tied to old row-file storage

## B+ Tree indexing

**Status:** Implemented

- persistent and page-backed
- now supports `RID` in main new insert path

## Display engine

**Status:** Implemented for standard table display

- menu display path works on page-based storage

## Buffer Pool Manager

**Status:** Not implemented

- only discussed in proposal / docs
- no active LRU page cache layer in current runtime

## Basic join support

**Status:** Not implemented

- no join executor found in current merged code

## Backend API layer

**Status:** Not implemented

- no active HTTP/TCP server integration in current main runtime

## Frontend

**Status:** Not implemented in runtime

- frontend is still planning-level, not codebase-integrated in core runtime

## 6. What Has Clearly Improved Compared to the Original Old Prototype

These are the most meaningful technical improvements visible in the merged code:

1. **Storage foundation is now real**
   - fixed-size pages
   - tuple slots
   - `RID`
2. **Insert path is redesigned**
   - no longer writes one row as one separate file in the active main insert flow
3. **Display path is redesigned**
   - table printing now reads pages and tuples
4. **Tuple serializer is added**
   - clean row-to-bytes and bytes-to-row conversion
5. **Persistent B+ Tree is stronger**
   - node pages are stored in `index.dat`
6. **Parser handling of quoted strings is improved**
   - especially around string literals and spaces

## 7. What Is Still Incomplete or Hybrid

This is the most important practical conclusion for the team.

The project is **not yet fully unified on one storage/query path**.

### New architecture already used in:

- insert
- display all rows
- tuple storage
- B+ Tree leaf values

### Old architecture still visible in:

- search
- `WHERE` execution
- some SQL select flow

### Result

The codebase is currently in a **migration stage**, not a fully finished DB engine.

## 8. Best One-Line Summary

MiniDB currently has a **working core CLI + metadata + persistent B+ Tree + page-based insert/display pipeline**, but **query execution, search unification, buffer pool, joins, and API/frontend are still incomplete or partially legacy**.

## 9. Recommended Next Technical Priorities

If the team wants to align the codebase with the original proposal, the most logical next steps are:

1. migrate `search.cpp` to page-based row fetch using `RID`
2. migrate `where.cpp` and SQL select execution to new page-based storage
3. remove remaining dependency on legacy `fileN.dat` row reads
4. add tests for insert-display-search correctness on the new storage path
5. implement buffer pool manager above `DiskManager`
6. only after that, move to joins and backend/API integration

## 10. Final Conclusion

Compared to the original proposal, the project has already implemented a meaningful part of the core database-engine internals:

- custom storage foundation
- serialization
- page-based tuple placement
- persistent B+ Tree indexing
- CLI table operations

But it is still not fully complete as a unified MiniDB engine.

The most accurate way to describe the current project is:

> a partially modernized MiniDB where the storage foundation is now strong, but query/search integration is still being migrated from legacy logic to the new page-based architecture.
