# Contributors

This document defines the **responsibility map**, **technical ownership**, and **project contribution flow** for the five-member team working on the mini database engine.

It ensures clarity on:

- who owns which system layer  
- how responsibilities are distributed  
- how different parts combine into a complete DBMS  

---

# Team Overview

| Member | Primary Role | Core Area | Impact |
|--------|-------------|----------|--------|
| **Aryan Saini** | Storage & Memory Architecture | Page design, RID, slotted pages | Builds the storage backbone |
| **Hemant Singh** | Disk Management | Disk persistence, page storage | Makes storage durable |
| **Anshdeep Singh** | B+ Tree + API | Indexing + API | Connects system to users & Develop Table Storage Structure |
| **Parth Khatri** | B+ Tree + Parser + SQL Layer | B+ Tree + Query processing | Improves query execution & Develop Table Storage Structure |
| **Aditya Sirsalker** | Parser + Frontend | Query parsing + UI | Makes system usable & Query Parsing |

---

# Contribution Breakdown

---

##  Aryan Saini

**Primary Role:** Storage and Memory Architecture

### Focus Area

- Page-based storage redesign  
- Row addressing using `RID(page_id, slot_id)`  
- Slotted page structure  
- Memory-side architecture  

### Core Responsibilities

- Define `PageHeader`, `SlotEntry`, and `RID`  
- Design tuple layout and serialization  
- Enable row lookup from pages  
- Prepare base for buffer pool  

### Key Work

- Page-based storage  
- Slotted page design  
- Tuple serialization  
- Storage structure planning  
- Buffer pool foundation  

### Impact

- Creates **core DBMS storage layer**
- Enables indexing using `RID`
- Converts project into real DBMS architecture  

---

## Hemant Singh

**Primary Role:** Disk Management

### Focus Area

- Disk persistence of pages  
- File-level page storage  

### Core Responsibilities

- Page creation and allocation  
- Page read/write operations  
- Maintain page files  
- Ensure durability  

### Key Work

- Disk manager  
- Page file handling  
- Persistent storage  
- Low-level I/O  

### Impact

- Makes storage **real and durable**
- Supports all higher-level modules  

---

## Anshdeep Singh

**Primary Role:** B+ Tree + API + Query Editor  

### Focus Area

- Indexing (B+ Tree)  
- Database API layer  
- Custom query editor interface  

### Core Responsibilities

- Implement B+ Tree structure and operations  
- Design API for database interaction  
- Build custom query editor UI  
- Ensure system usability through API  

### Key Work

- B+ Tree indexing logic  
- API endpoints for DB operations  
- Query editor interface  
- System-level interaction layer  



### Impact

- Enables **external interaction with DB**
- Provides **developer-friendly interface**
- Adds **usability + accessibility**
- Adds **Structure to Data Storage**

---

## Parth Khatri

**Primary Role:** B+ Tree + Parser + SQL Layer  

### Focus Area

- B+ Tree indexing  
- SQL query handling  
- Parser integration and development

### Core Responsibilities

- Work on B+ Tree implementation  
- Implement SQL-like query commands (`SELECT`, `INSERT`, `WHERE` , `JOIN` etc.)  
- Integrate parser with execution flow  
- Improve query execution efficiency  

### Key Work

- B+ Tree logic  
- SQL command handling  
- Query execution flow  
- Parser integration  


### Impact

- Improves **query performance**
- Adds **real SQL Commands via Parser**
- Connects parser → execution → indexing  
- Adds **Structure to Data Storage**

---

## Aditya Sirsalker

**Primary Role:** Parser + API Integration 

### Focus Area

- Query parsing  
- Frontend interface  
- Parser Development

### Core Responsibilities

- Work on parser improvements  
- Handle query validation  
- Build frontend UI  
- Connect frontend with backend API  

### Key Work

- Parser logic  
- Syntax validation  
- Frontend development  
- API integration  


### Impact

- Improves **usability**
- Provides **visual interface**
- Connects user → system  



---

# Project Layer Ownership

| Layer | Owner | Responsibility | Importance |
|------|------|---------------|------------|
| Storage Layer | Aryan Saini | Page structure, RID, layout | Foundation |
| Disk Layer | Hemant Singh | Persistence & disk I/O | Durability |
| Index Layer | Anshdeep + Parth | B+ Tree indexing | Performance |
| API Layer | Anshdeep Singh | DB interaction interface | Accessibility |
| Query Layer | Parth + Aditya | SQL + parsing | Functionality |
| Frontend Layer | Aditya Sirsalker | UI | Usability |

---

# Final Summary

- **Aryan Saini** → builds the **core storage architecture**
- **Hemant Singh** → ensures **data persistence on disk**
- **Anshdeep Singh** → connects system via **API + indexing + editor**
- **Parth Khatri** → enables **parse + query execution + indexing logic + SQL commands**
- **Aditya Sirsalker** → delivers **parser + frontend usability + SQL Commands**

---
