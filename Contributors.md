# Contributors

This document summarizes the main ownership areas of the five-member project team in simple words.

The goal is to show who is leading which part of the database engine and what impact that work has on the final system.

---

## Team Contribution Overview

| Member | Core Role | What This Means In Simple Words | Impact On The Project |
| --- | --- | --- | --- |
| **Aryan Saini** | **Storage Architecture and Memory Flow** | Designing how the database stores rows in pages instead of separate files, defining `RID(page_id, slot_id)`, shaping slotted-page storage, and driving the logic that later supports clean RAM-side page handling. | Builds the base of the new DBMS-style engine so rows, indexes, and memory management can work in a proper structured way instead of a file-per-row model. |
| **Hemant Singh** | **Disk Management and Page Persistence** | Handling how pages are created, saved, loaded, and tracked on disk so the storage design can actually work in practice. | Turns the storage design into a working disk layer that the database can rely on for real reads and writes. |
| **Anshdeep Singh** | **Core Integrator** | Connecting the major modules of the project so storage, parser, indexing, and execution do not remain isolated parts. | Keeps the system unified and makes sure the project behaves like one engine rather than separate incomplete components. |
| **Parth Khatri** | **B+ Tree Indexing** | Building and adapting the B+ Tree logic used for faster key-based searching and later linking it with row addresses. | Makes lookups efficient and gives the engine a real indexing backbone instead of only brute-force searching. |
| **Adity Sirsalker** | **Query Handling and SQL-Side Flow** | Focusing on the query-facing side, including parser-side improvements, supported input flow, and user-facing query behavior. | Helps make the engine usable from the command side by improving how queries are understood, validated, and routed into execution. |

---

## Linked Subteam Note

### Aryan Saini and Hemant Singh

These two roles are closely linked.

| Subteam | Combined Responsibility | Why This Pairing Matters |
| --- | --- | --- |
| **Aryan Saini + Hemant Singh** | Aryan defines how page-based storage and memory-side organization should work, while Hemant handles the low-level disk management needed to make that storage model run properly. | Together, they cover the full storage path: from **how data should be organized** to **how that organized data is physically read and written**. |

---

## Project Layer Ownership

| Project Layer | Primary Owner(s) | Main Responsibility | Why It Matters |
| --- | --- | --- | --- |
| **Storage Model** | **Aryan Saini** | Define page-based storage, row addressing, and the structure of stored data. | This is the foundation on which indexing and execution depend. |
| **Disk Layer** | **Hemant Singh** | Build and maintain the page read/write layer and persistent storage path. | Without this, the new storage model cannot actually run. |
| **System Integration** | **Anshdeep Singh** | Bring all modules together and maintain coherence across components. | Prevents the codebase from splitting into unrelated pieces. |
| **Index Layer** | **Parth Khatri** | Build B+ Tree logic for efficient searching. | Enables scalable lookup performance. |
| **Query / Input Layer** | **Adity Sirsalker** | Improve parser-side and query-side behavior. | Makes the engine practical to use from the user side. |

---

## Short Role Impact Summary

- **Aryan Saini** is shaping the storage direction of the engine so the project grows from a basic prototype into a more realistic DBMS design.
- **Hemant Singh** is grounding that design in an actual disk-management path so storage can persist and scale properly.
- **Anshdeep Singh** is the connector who helps the separate technical modules function as one complete project.
- **Parth Khatri** is responsible for the indexing strength of the system through B+ Tree work.
- **Adity Sirsalker** supports the query-facing side so user input and execution flow become more robust and usable.

---

## Editable Template Notes

If the team later wants to refine this file for final submission or presentation, these are the easiest parts to update:

- the exact wording of each ownership title
- the final query/parser scope for **Adity Sirsalker**
- the final storage-memory wording for **Aryan Saini**
- the exact integration scope handled by **Anshdeep Singh**
- any final collaboration links between indexing, parser, and execution roles
