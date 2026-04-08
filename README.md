# 🗄️ MiniDB Engine (Relational Database from Scratch)

> A simplified, custom-built relational database engine designed to replicate the core internal functionalities of modern Database Management Systems (DBMS). 

[![Language: C++](https://img.shields.io/badge/Language-C++-blue.svg)](https://isocpp.org/)
[![Build: Make](https://img.shields.io/badge/Build-Make-orange.svg)]()

Unlike projects that act as wrappers around existing databases (like SQLite or MySQL), this project is built entirely **from scratch**. It features custom implementation of disk-based storage management, B+ Tree indexing, LRU memory caching, and a homegrown query execution engine.

---

## ✨ Key Features

### 🛠️ Core Capabilities
* **Custom Storage Engine:** Data is stored on the physical disk using block-based file storage. Each table is allocated separate files/directories to manage its blocks.
* **B+ Tree Indexing:** Implements a multi-level B+ Tree structure for primary keys, enabling highly efficient $O(\log n)$ indexed lookups.
* **Query Parser & Execution:** A custom SQL-like execution engine supporting `SELECT`, `INSERT`, `CREATE`, and `DROP` commands, alongside conditional filtering (`WHERE`, `=`, `<`, `>`, `AND`, `OR`).
* **Metadata Management:** Maintains schema information (table names, column types, record counts) in dedicated, fast-access metadata files.
* **Search Algorithms:** * *Indexed Search:* Ultra-fast lookups using B+ Tree traversal for primary keys.
  * *Brute-Force Search:* Full table scans for non-indexed columns with execution time monitoring.

### 🚀 Advanced Extensions (In Active Development)
* **Buffer Pool Manager (LRU):** An in-memory caching system that minimizes disk I/O by keeping frequently accessed disk pages/blocks in RAM using a Least Recently Used (LRU) policy.
* **Basic Join Support:** Implementation of Nested Loop Joins to support simple multi-table queries.
* **Backend API Layer:** HTTP/TCP socket interface allowing external applications to connect and execute queries.

---

## 🏗️ System Architecture

1. **Storage Manager:** Handles raw disk read/writes, fetching data in fixed-size blocks rather than line-by-line to mimic real DBMS page architecture.
2. **Buffer Manager:** Sits between the Storage Manager and the Execution Engine, caching pages in memory.
3. **Index Manager:** Maintains the B+ Tree structures. Each node contains a maximum of 50 entries, splitting and merging dynamically as data is inserted or deleted.
4. **Query Processor:** Parses incoming string queries into abstract syntax trees (ASTs) and plans the execution path.

---

## 💻 Tech Stack

* **Core Engine:** `C++` (Data Structures, File I/O, Memory Management)
* **Networking (Upcoming):** C++ Sockets / Node.js HTTP Server
* **Frontend Demo (Upcoming):** `React.js`
* **Build System:** `Make` / `GCC`

---

## 📝 Supported SQL Queries

The system currently parses and executes the following SQL-like syntax:

```sql
-- DDL Commands
SHOW TABLES;
CREATE TABLE table_name (id INTEGER PRIMARY KEY, name VARCHAR);
DROP TABLE table_name;

-- DML Commands
INSERT INTO table_name VALUES (1, 'John Doe');

-- DQL Commands (Data Querying)
SELECT * FROM table_name;
SELECT id, name FROM table_name;
SELECT * FROM table_name WHERE id = 1;
SELECT id, name FROM table_name WHERE id > 10 AND name = 'Alice';