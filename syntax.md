# 📖 MiniDB Query Syntax Reference

> Supported SQL-like syntax for the MiniDB Engine. Currently, only `CREATE TABLE` and `SELECT` are functional.

---

## CREATE TABLE

Creates a new table with the specified columns and types.

### Syntax

```sql
CREATE TABLE table_name (column1 TYPE, column2 TYPE, ...);
```

### Supported Column Types

| Type | Description | Size |
|------|-------------|------|
| `INT` | Integer values | Fixed |
| `VARCHAR` | Variable-length string | Default size (unspecified) |
| `VARCHAR(n)` | Variable-length string | Max `n` characters |

### Examples

```sql
-- Creating a table with default string sizes
create table Students (id INT, name VARCHAR);

-- Creating a table with explicit string sizes
CREATE TABLE Employees (emp_id INT, email VARCHAR(100), department VARCHAR(15));
```

### Notes

- Keywords are **case-insensitive** (`CREATE` = `create`)
- Each column definition is `column_name TYPE` or `column_name TYPE(size)`
- Column definitions are separated by commas inside parentheses
- `VARCHAR` without a size uses an engine-default max length
- `VARCHAR(n)` enforces a maximum of `n` characters

---

## SELECT

Retrieves rows from an existing table, with optional column filtering.

### Syntax

```sql
SELECT * FROM table_name;
SELECT column1, column2 FROM table_name;
```

### Examples

```sql
-- Select all columns from Students
select * from Students;

-- Select specific columns from Employees
select email, department from Employees;
```

### Notes

- Use `*` to retrieve all columns
- List specific column names (comma-separated) to retrieve a subset
- Keywords are **case-insensitive** (`SELECT` = `select`)
- Column names in the select list must exist in the table

---

## ⚠️ Not Yet Supported

The following queries are **parsed but not yet functional**:

```sql
-- These do NOT work in the current version
INSERT INTO table_name VALUES (...);
SELECT * FROM table_name WHERE column = value;
DROP TABLE table_name;
SHOW TABLES;
```

---

*For full feature roadmap, see [README.md](./README.md).*