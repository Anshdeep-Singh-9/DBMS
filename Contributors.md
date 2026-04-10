# Contributors

This file presents the project contribution map for the five-member team in a clean and simple format.

The purpose is to clearly show:

- who is leading which technical area
- what features each member is expected to build
- how each role affects the final mini database engine

---

## Team At A Glance

| Member | Main Role | Core Area | Project Impact |
| --- | --- | --- | --- |
| **Aryan Saini** | **Storage and Memory Architecture Lead** | Page-based storage, `RID`, slotted pages, tuple layout, buffer pool direction | Builds the core internal storage foundation of the engine |
| **Hemant Singh** | **Disk Management Lead** | Page persistence, page allocation, storage file handling | Makes the designed storage model actually work on disk |
| **Anshdeep Singh** | **Core Integrator** | Module linking, overall system flow, project coherence | Keeps the project unified and integration-ready |
| **Parth Khatri** | **Indexing Lead** | B+ Tree design and key-based lookup flow | Makes searches faster and indexing practical |
| **Adity Sirsalker** | **Query and Parser Flow** | Query-side behavior, parser-side improvements, user query path | Improves how the engine understands and routes queries |

---

## Contribution Breakdown

### Aryan Saini

**Primary Role:** Storage and Memory Architecture Lead

**Focus Area**

- redesign the storage model from row-per-file to page-based storage
- define how rows are addressed using `RID(page_id, slot_id)`
- shape how rows are placed inside slotted pages
- drive the memory-side design needed before buffer pool behavior works properly

**Core Responsibilities**

- define the new page-based storage direction
- define `PageHeader`, `SlotEntry`, and `RID`
- shape row layout and tuple serialization format
- support the path from stored bytes -> page -> row lookup
- prepare the design base for later buffer pool integration

**Key Features / Work Areas**

- page-based table storage
- row addressing using `RID`
- slotted-page structure
- tuple serialization and deserialization
- storage-side row organization
- memory-side page flow planning
- **buffer pool / RAM management foundation**

**Linked Collaboration**

- works closely with **Hemant Singh**
- Aryan defines how data should be organized in memory and inside pages
- Hemant supports how those pages are physically created, stored, and read from disk

**In Simple Words**

- Aryan is helping the project move from "save each row in a separate file"
- to "store many rows properly inside managed pages like a real DBMS"

**Project Impact**

- creates the storage backbone of the new engine
- makes future indexing cleaner because B+ Trees can later point to `RID`
- makes buffer pool work meaningful because memory caching needs fixed-size pages
- turns the project from a basic prototype into a more serious DBMS design

---

### Hemant Singh

**Primary Role:** Disk Management Lead

**Focus Area**

- manage how pages are stored and retrieved from disk
- support file-level page allocation and persistence
- provide the disk-side support needed by the new storage model

**Core Responsibilities**

- handle page creation on disk
- handle page read and page write flow
- maintain storage files that hold fixed-size pages
- support stable persistence for the redesigned storage engine

**Key Features / Work Areas**

- disk manager logic
- page file handling
- persistent page storage
- low-level storage read/write operations

**In Simple Words**

- Hemant is making sure the new storage pages do not remain only a design idea
- he is helping them actually live safely on disk

**Project Impact**

- turns storage design into real persistent storage behavior
- supports every higher-level feature that depends on stored pages
- enables the project to move beyond temporary in-memory logic

---

### Anshdeep Singh

**Primary Role:** Core Integrator

**Focus Area**

- connect the major modules of the database engine
- keep the project architecture coherent as separate features are added

**Core Responsibilities**

- coordinate module integration
- align storage, indexing, parser, and execution paths
- reduce fragmentation across team contributions
- help maintain overall project structure

**Key Features / Work Areas**

- system integration
- project-level wiring
- architecture-level coherence
- module handoff and merge alignment

**In Simple Words**

- Anshdeep helps make sure the project behaves like one complete engine
- not five separate incomplete parts

**Project Impact**

- reduces integration problems
- keeps the final system usable as one coherent product
- helps ensure separate features connect properly

---

### Parth Khatri

**Primary Role:** B+ Tree and Indexing Lead

**Focus Area**

- efficient indexed search
- B+ Tree behavior for key-based access
- support future connection between indexes and row addresses

**Core Responsibilities**

- work on B+ Tree logic
- support exact-match key lookup
- help align indexing with the new `RID`-based storage direction
- strengthen search efficiency over brute-force scanning

**Key Features / Work Areas**

- B+ Tree node behavior
- key insertion and lookup
- indexed search path
- later `key -> RID` mapping support

**In Simple Words**

- Parth is handling the part that helps the database find things faster
- instead of scanning every row one by one

**Project Impact**

- improves search performance
- gives the project a real indexing layer
- supports scalable lookup behavior as the engine grows

---

### Adity Sirsalker

**Primary Role:** Query Handling and Parser Flow

**Focus Area**

- improve how the system accepts and understands queries
- support parser-side and query-side usability

**Core Responsibilities**

- improve query handling flow
- support parser-related fixes and improvements
- help shape the supported SQL-like input path
- improve user-facing query behavior and validation

**Key Features / Work Areas**

- parser-side improvement
- query input behavior
- syntax and query handling fixes
- user-facing query support

**In Simple Words**

- Adity is helping the engine better understand what the user types
- and route that input properly into execution

**Project Impact**

- makes the database easier to use
- improves supported query flow
- helps turn internal modules into a usable command/query system

---

## Linked Subteam: Storage Path

### Aryan Saini + Hemant Singh

This is the storage subteam of the project.

| Subteam Area | Aryan Saini | Hemant Singh | Combined Impact |
| --- | --- | --- | --- |
| **Storage Path** | Defines how data should be arranged in pages, how rows are addressed, and how memory-side page logic should work. | Handles the disk-side operations that store, load, and persist those pages. | Together they cover the full path from **storage design** to **storage execution on disk**. |

### Why this pairing matters

- Aryan shapes the structure of data inside the system
- Hemant makes that structure persist correctly on disk
- together they make later features like buffer pool and indexing possible

---

## Project Layer Ownership

| Layer | Owner | Main Work | Why It Matters |
| --- | --- | --- | --- |
| **Storage Model Layer** | **Aryan Saini** | Page layout, `RID`, slotted pages, tuple layout | This is the base of the DBMS engine |
| **Disk Layer** | **Hemant Singh** | Disk manager and page persistence | This makes storage real and durable |
| **Integration Layer** | **Anshdeep Singh** | Cross-module alignment and system wiring | This keeps the project connected |
| **Index Layer** | **Parth Khatri** | B+ Trees and indexed lookup | This improves efficiency |
| **Query Layer** | **Adity Sirsalker** | Query flow and parser-facing improvements | This improves usability |

---

## Final Impact Summary

- **Aryan Saini** leads the storage direction and memory-side design needed to transform the project into a real page-based DBMS-style engine.
- **Hemant Singh** makes that storage direction practical by handling the disk-level page operations required to persist and retrieve data.
- **Anshdeep Singh** keeps all major modules connected so the final codebase behaves like one coherent system.
- **Parth Khatri** strengthens the project with indexed access through B+ Tree-based search logic.
- **Adity Sirsalker** improves the query-facing side so the engine becomes easier to use and more stable in its SQL-like flow.

---

## Template Notes For Later Editing

This file is ready to be refined later once all modules are complete.

Possible future updates:

- add final completed feature names under each member
- add PR links or issue references under each role
- replace template-style wording with final submitted work
- add a final "team workflow" or "collaboration graph" section if needed
