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
1. Show all tables in database
2. Create table        ← type a CREATE TABLE query
3. Insert into table   ← interactive prompt, column by column
4. Drop table
5. Display table contents  ← type a SELECT query
6. Search  (work in progress)
7. Print metadata of a table
8. Help
9. Quit
```

---

## Architecture

```
  SQL Query (typed in terminal)
           │
           ▼
  ┌─────────────────┐
  │   Query Parser  │  Tokenizes input → dispatches to handler
  └────────┬────────┘
           │
           ▼
  ┌─────────────────┐
  │ Execution Layer │  create / select / insert / drop
  └────────┬────────┘
           │
      ┌────┴────┐
      │         │
      ▼         ▼
  ┌───────┐  ┌──────────────┐
  │ Index │  │ Buffer Pool  │  LRU cache — avoids redundant disk reads
  │Manager│  │   Manager    │
  └───┬───┘  └──────┬───────┘
      │              │
      └──────┬───────┘
             │
             ▼
  ┌─────────────────┐
  │ Storage Manager │  Fixed-size block I/O  (4 KB pages)
  └─────────────────┘
             │
             ▼
       table/<name>/
       ├── data.dat     ← slotted pages holding row data
       ├── index.dat    ← B+ Tree node pages
       └── meta.dat     ← schema (column names, types, sizes)
```

| Layer | File | What it does |
|---|---|---|
| Query Parser | `parser.cpp` | Tokenizes SQL strings, routes to handler |
| Execution Layer | `create.cpp`, `display.cpp`, `insert.cpp` | Runs DDL / DML operations |
| Index Manager | `BPtree.cpp` | B+ Tree on primary key, persisted to `index.dat` |
| Buffer Pool Manager | `buffer_pool_manager.cpp` | LRU page cache between execution and disk |
| Data Page | `data_page.cpp` | Slotted-page layout inside each 4 KB block |
| Disk Manager | `disk_manager.cpp` | Raw page read/write at byte offsets in a file |
| Tuple Serializer | `tuple_serializer.cpp` | Packs/unpacks row values to/from raw bytes |

---

## Features

### Storage Engine

- **Block-based disk I/O** — data lives in fixed-size 4 KB pages inside `data.dat`, one file per table, matching real DBMS page architecture.
- **Slotted page layout** — each page has a header, a slot directory growing from the front, and tuple data packed from the back. Rows are addressed by `(page_id, slot_id)` — a Record ID (RID).
- **Tuple serialization** — rows are serialized to raw bytes on insert and deserialized back to typed values on read.

### B+ Tree Index

- Multi-level B+ Tree on the primary key (always the first `INT` column), persisted to `index.dat`.
- `O(log n)` key lookup, returning the RID of the matching row.
- Nodes hold up to 50 entries and split/merge automatically.
- Leaf nodes are doubly linked for range scan support.

### Buffer Pool Manager

- In-memory LRU page cache sits between the execution layer and the disk.
- Pages are pinned on fetch and unpinned after use; dirty pages are flushed back to disk.
- Reduces redundant disk reads when the same page is accessed multiple times.

### Query Parser

- Accepts freeform SQL-like strings from the terminal.
- `CREATE TABLE` and `SELECT` are handled via the query prompt (menu options 2 and 5).
- `INSERT` and `DROP` use interactive prompts (menu options 3 and 4).

---

## Supported Operations

| Operation | How to invoke | Status |
|---|---|---|
| `SHOW TABLES` | Menu option 1 | ✅ Working |
| `CREATE TABLE` | Menu option 2 → type query | ✅ Working |
| `INSERT` | Menu option 3 → interactive prompt | ✅ Working |
| `DROP TABLE` | Menu option 4 → enter table name | ✅ Working |
| `SELECT *` / `SELECT cols` | Menu option 5 → type query | ✅ Working |
| View table metadata | Menu option 7 | ✅ Working |
| REST API Querying | `make api` → `./server` | ✅ Working |
| Search / `WHERE` filtering | Menu option 6 | 🚧 In progress |

---

## API Integration

MiniDB now includes a REST API server built with the [Crow](https://github.com/CrowCpp/Crow) microframework. This allows you to query your database from external languages like Node.js, Python, or via web browsers.

### Quick Start API
1. Build the API: `make api`
2. Start the server: `./server`
3. Query a table: `curl http://localhost:18080/table/Students`

For more details, see the [API Documentation](./Documentation/api.md).

---

## Query Syntax (for options 2 and 5)

```sql
-- Create
CREATE TABLE students (id INT, name VARCHAR);
CREATE TABLE employees (emp_id INT, email VARCHAR(100), dept VARCHAR(15));

-- Select
SELECT * FROM students;
SELECT email, dept FROM employees;
```

> See [`SYNTAX.md`](./SYNTAX.md) for the full syntax reference.

---

## File Layout

```
DBMS/
├── src/
│   ├── main.cpp                 # Entry point, menu loop, login
│   ├── parser.cpp               # SQL tokenizer and query router
│   ├── create.cpp               # CREATE TABLE logic
│   ├── insert.cpp               # INSERT (interactive prompt)
│   ├── display.cpp              # SELECT + SHOW TABLES logic
│   ├── BPtree.cpp               # B+ Tree index
│   ├── disk_manager.cpp         # Page-based file I/O
│   ├── buffer_pool_manager.cpp  # LRU buffer pool
│   ├── data_page.cpp            # Slotted page layout
│   ├── tuple_serializer.cpp     # Row serialization
│   └── ...
├── include/                     # Header files
├── table/                       # Created at runtime — one folder per table
│   └── <table_name>/
│       ├── data.dat
│       ├── index.dat
│       └── meta.dat
├── Makefile
├── README.md
└── SYNTAX.md
```

---

## Tech Stack

| Component | Technology |
|---|---|
| Core Engine | C++17 — data structures, file I/O, memory management |
| Build System | Make / GCC |
| Interface | Terminal (interactive menu + SQL prompt) |

---

## Project Goals

MiniDB is a systems programming project built to understand how databases actually work. Every component — the buffer pool, the B+ Tree, the slotted page format, the tuple serializer — is implemented from scratch to mirror the internal design of production engines like PostgreSQL or InnoDB.

---

<div align="center">
<sub>Built from scratch · No database dependencies · Terminal-based · C++17</sub>
</div>
