<div align="center">

# 🗄️ MiniDB Engine

### A Relational Database Built From Scratch in C++

*Disk storage · B+ Tree indexing · LRU buffer pool · Custom query engine — zero dependencies on existing databases*

[![Language: C++](https://img.shields.io/badge/Language-C++17-blue?style=flat-square&logo=cplusplus)](https://isocpp.org/)
[![Build: Make](https://img.shields.io/badge/Build-Make%20%2F%20GCC-orange?style=flat-square)]()
[![Interface: Terminal](https://img.shields.io/badge/Interface-Terminal-black?style=flat-square)]()
[![Status: Active](https://img.shields.io/badge/Status-Active%20Development-green?style=flat-square)]()

</div>

---

## What Is MiniDB?

MiniDB is a from-scratch relational database engine — not a wrapper around SQLite or MySQL, but a ground-up system that replicates how real DBMSs work internally.

It runs entirely in the **terminal**. You log in, get a menu, and interact through a numbered interface or by typing SQL-like queries. Under the hood, it manages its own disk storage, in-memory buffer pool, B+ Tree index structures, and a SQL-like query parser.

---

## Getting Started

### Build

```bash
make
```

### Run

```bash
./minidb -u <username> -p
# Enter password when prompted (default: pass)
```

### Terminal Menu

Once logged in, MiniDB presents a numbered menu:

```
1. Query Console  (CREATE / INSERT / SELECT / SHOW / DROP)
2. Search table / search inside table
3. Print metadata of a table
4. Help
5. Quit
```

All SQL-like operations are now routed through the **Query Console** (option 1). Type `BACK` or `EXIT` inside the console to return to the main menu.

---

## Architecture

```
  SQL Query (typed in Query Console)
           │
           ▼
  ┌─────────────────┐
  │   Query Parser  │  Tokenizes input → dispatches to handler
  │   (parser.cpp)  │  Preserves quoted string values correctly
  └────────┬────────┘
           │
    ┌──────┴──────┬──────────────┬──────────┐
    ▼             ▼              ▼          ▼
 create.cpp   insert.cpp    display.cpp  where.cpp
 (DDL)        (DML insert)  (SELECT/SHOW) (WHERE filter)
    │             │              │
    └──────┬──────┘              │
           ▼                     ▼
  ┌────────────────┐   ┌──────────────────┐
  │  Index Manager │   │  Buffer Pool Mgr │  LRU cache
  │  (BPtree.cpp)  │   │  (buffer_pool_   │  avoids redundant
  └───────┬────────┘   │   manager.cpp)   │  disk reads
          │            └────────┬─────────┘
          └──────────┬──────────┘
                     ▼
          ┌─────────────────────┐
          │   Storage Manager   │  Fixed-size 4 KB page I/O
          │  (disk_manager.cpp) │
          └─────────────────────┘
                     │
                     ▼
              table/<name>/
              ├── data.dat     ← slotted pages holding row data
              ├── index.dat    ← B+ Tree node pages
              └── met          ← schema (column names, types, sizes)
```

| Layer | File | What it does |
|---|---|---|
| Query Parser | `parser.cpp` | Tokenizes SQL strings, routes to handler; handles quoted values |
| DDL Execution | `create.cpp` | `CREATE TABLE` with primary key constraint, initializes B+ Tree |
| DML Insert | `insert.cpp` | Buffer-pool-aware insert, updates index with RID |
| DML Select | `display.cpp` | `SELECT`, `SHOW TABLES`, column projection |
| WHERE Filter | `where.cpp` | B+ Tree point lookup (PK) or linear scan (non-PK) |
| Index Manager | `BPtree.cpp` | Persistent B+ Tree on `index.dat` |
| Buffer Pool Manager | `buffer_pool_manager.cpp` | LRU page cache between execution and disk |
| Data Page | `data_page.cpp` | Slotted-page layout inside each 4 KB block |
| Disk Manager | `disk_manager.cpp` | Raw page read/write at byte offsets in a file |
| Tuple Serializer | `tuple_serializer.cpp` | Packs/unpacks row values to/from raw bytes |
| File Handler | `file_handler.cpp` | Metadata read/write, table registry management |

---

## Features

### Storage Engine

- **Block-based disk I/O** — data lives in fixed-size 4 KB pages inside `data.dat`, one file per table, matching real DBMS page architecture.
- **Slotted page layout** — each page has a header, a slot directory growing from the front, and tuple data packed from the back. Rows are addressed by `(page_id, slot_id)` — a Record ID (RID).
- **Tuple serialization** — rows are serialized to raw bytes on insert and deserialized back to typed values on read. VARCHARs are stored with a `uint16_t` length prefix (variable-length, not padded).
- **Eager flush** — `DiskManager` flushes writes immediately after each page write and on close, ensuring persistence for testing and correctness.

### B+ Tree Index

- Multi-level B+ Tree on the primary key (always the first `INT` column), persisted to `index.dat`.
- `O(log n)` key lookup, returning the RID `(page_id, slot_id)` of the matching row.
- Nodes hold up to `MAX_KEYS` entries and split automatically on overflow.
- Leaf nodes are **doubly linked** (`next_page_id` / `prev_page_id`) for range scan support.
- Page 0 of `index.dat` stores the root page ID as metadata; the tree is fully serialized to disk.

### Buffer Pool Manager

- In-memory **LRU page cache** sits between the execution layer and the disk.
- Pages are **pinned** on fetch and **unpinned** after use; dirty pages are flushed back to disk.
- Reduces redundant disk reads when the same page is accessed multiple times within a query.
- Used by both `insert.cpp` (write path) and `display.cpp` (read path).

### Query Parser

- Accepts freeform SQL-like strings from the Query Console.
- Handles **quoted string values** (`'...'` and `"..."`) correctly — preserves original case and strips quotes before processing.
- Lowercases only SQL keywords; table names, column names, and varchar values retain their original case.
- `keyword_lower_copy` ensures only keywords outside quotes are lowercased during parsing.

### WHERE Clause Execution

- Automatically selects the optimal search strategy:
  - **B+ Tree point lookup** when the WHERE column is the primary key (INT).
  - **Linear scan** over all pages when filtering on a non-primary-key column.
- Supports `SELECT col1, col2 FROM table WHERE col = value` with column projection.

### Primary Key Constraint

- The first column of every table **must be `INT`** — enforced at `CREATE TABLE` time.
- Duplicate primary key values are rejected at insert time by checking the B+ Tree before writing.

---

## Supported Operations

| Operation | Syntax | Status |
|---|---|---|
| `SHOW TABLES` | `SHOW TABLES;` | ✅ Working |
| `CREATE TABLE` | `CREATE TABLE name (col TYPE, ...);` | ✅ Working |
| `INSERT INTO` | `INSERT INTO name VALUES (v1, v2, ...);` | ✅ Working |
| `DROP TABLE` | `DROP TABLE name;` | ✅ Working |
| `SELECT *` | `SELECT * FROM name;` | ✅ Working |
| `SELECT cols` | `SELECT col1, col2 FROM name;` | ✅ Working |
| `SELECT WHERE (PK)` | `SELECT * FROM name WHERE id = 1;` | ✅ B+ Tree lookup |
| `SELECT WHERE (non-PK)` | `SELECT * FROM name WHERE name = Raj;` | ✅ Linear scan |
| View table metadata | Menu option 3 | ✅ Working |
| REST API Querying | `make api` → `./server` | ✅ Working |
| Search (menu) | Menu option 2 | 🚧 In progress |

---

## Query Syntax

```sql
-- Show all tables
SHOW TABLES;

-- Create a table (first column must be INT — it is the primary key)
CREATE TABLE students (id INT, name VARCHAR(50), dept VARCHAR(20));
CREATE TABLE employees (emp_id INT, email VARCHAR(100), salary INT);

-- Insert a row
INSERT INTO students VALUES (1, Anshdeep Singh, CSE);
INSERT INTO students VALUES (2, "Aditya Sirsalkar", "CSE");   -- quotes optional

-- Select all columns
SELECT * FROM students;

-- Select specific columns
SELECT name, dept FROM students;

-- Filter with WHERE (uses B+ Tree if filtering on primary key)
SELECT * FROM students WHERE id = 1;

-- Filter on non-primary-key column (uses linear scan)
SELECT * FROM students WHERE dept = CSE;

-- Drop a table
DROP TABLE students;
```

**Notes:**
- SQL keywords (`SELECT`, `FROM`, `WHERE`, etc.) are **case-insensitive**.
- Table names and column names are **case-sensitive**.
- VARCHAR values keep their original case exactly as typed.
- Quotes (`'` or `"`) are optional for VARCHAR values but recommended for multi-word strings.
- The first column must be `INT` (used as the primary key for B+ Tree indexing).
- INSERT values must match the column order defined in `CREATE TABLE`.

---

## Data Types

| Type | Syntax variants | Storage |
|---|---|---|
| Integer | `INT`, `INTEGER` | 4 bytes (`int32_t`) |
| Variable-length string | `VARCHAR`, `VARCHAR(n)` | 2-byte length prefix + actual string bytes |

> If no size is specified for `VARCHAR`, `MAX_VARCHAR` is used as the default maximum length.

---

## File Layout

```
DBMS/
├── src/
│   ├── main.cpp                 # Entry point, login, menu loop, query console
│   ├── parser.cpp               # SQL tokenizer and query router
│   ├── create.cpp               # CREATE TABLE — validates PK, stores metadata, initializes index
│   ├── insert.cpp               # INSERT — buffer pool write path, B+ Tree update
│   ├── display.cpp              # SELECT, SHOW TABLES, table metadata display
│   ├── where.cpp                # WHERE clause — B+ Tree lookup or linear scan
│   ├── BPtree.cpp               # B+ Tree index (persisted to index.dat)
│   ├── disk_manager.cpp         # Page-based file I/O (4 KB fixed pages)
│   ├── buffer_pool_manager.cpp  # LRU buffer pool (in-memory page cache)
│   ├── data_page.cpp            # Slotted page layout
│   ├── tuple_serializer.cpp     # Row serialization/deserialization
│   └── file_handler.cpp         # Metadata I/O, table registry (table_list)
├── include/                     # Header files
├── table/                       # Created at runtime — one folder per table
│   ├── table_list               # Central registry of all table names
│   └── <table_name>/
│       ├── data.dat             # Row data (slotted pages)
│       ├── index.dat            # B+ Tree nodes
│       └── met                  # Table schema (binary struct)
├── Makefile
├── README.md
└── SYNTAX.md
```

---

## API Integration

MiniDB includes a REST API server built with the [Crow](https://github.com/CrowCpp/Crow) microframework, allowing external access from Node.js, Python, or a browser.

```bash
# Build the API server
make api

# Start the server
./server

# Query a table
curl http://localhost:18080/table/Students
```

See [`Documentation/api.md`](./Documentation/api.md) for full endpoint reference.

---

## Tech Stack

| Component | Technology |
|---|---|
| Core Engine | C++17 — data structures, file I/O, memory management |
| Build System | Make / GCC |
| Interface | Terminal (interactive menu + SQL Query Console) |
| REST API | Crow microframework (optional) |

---

## Project Goals

MiniDB is a systems programming project built to understand how databases actually work at the implementation level. Every component — the buffer pool, the B+ Tree, the slotted page format, the tuple serializer, the query parser — is implemented from scratch to mirror the internal design of production engines like PostgreSQL or InnoDB.

---

<div align="center">
<sub>Built from scratch · No database dependencies · Terminal-based · C++17</sub>
</div>
