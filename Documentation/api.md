# MiniDB REST API Documentation

The `api/` folder contains the source code for the MiniDB REST API server. This server acts as a bridge, allowing external applications (written in languages like Node.js, Python, or Go) to interact with MiniDB using standard HTTP requests and JSON data.

---

## 🚀 Features

- **Language Agnostic**: Use MiniDB from any language that supports HTTP.
- **JSON Support**: Data is returned in a structured, easy-to-parse JSON format.
- **Multithreaded**: Built on the Crow microframework for high-performance concurrent handling.
- **Automatic Setup**: Dependencies are managed automatically via the Makefile.

---

## 🛠️ Compilation & Setup

The API depends on the **Crow** and **Asio** libraries. The project's Makefile is configured to download these automatically if they are missing.

### Compile the API Server
Run the following command from the project root:
```bash
make api
```
This will:
1. Build the core MiniDB object files.
2. Download Crow and Asio headers to `include/` (if not present).
3. Create an executable named `server` in the root directory.

---

## 🏃 Running the Server

Start the API server by running:
```bash
./server
```
By default, the server runs on **port 18080**.

---

## 📡 API Endpoints

### 1. Health Check
Verify if the API is running.

- **URL**: `/health`
- **Method**: `GET`
- **Success Response**: `MiniDB API is running!`

### 2. Get Table Contents
Retrieve all records from a specific table in JSON format.

- **URL**: `/table/<table_name>`
- **Method**: `GET`
- **Success Response**: A JSON array of objects, where each object represents a row.
- **Error Response**: `{"error": "Table not found"}` or `{"error": "Could not open data file"}`

---

## 💻 Example Usage

### Using cURL
```bash
curl http://localhost:18080/table/Students
```

### Using Node.js
A helper script is provided in `test/client_test.js`. To use it:
```bash
node test/client_test.js Students
```

---

## 📂 Directory Structure

- `api/server.cpp`: The main entry point for the REST API.
- `include/crow_all.h`: The Crow microframework (downloaded automatically).
- `include/asio/`: The Asio networking library (downloaded automatically).

---

## 📝 Technical Note: Statelessness
The API is **stateless**. This means each request is independent. The server does not maintain a persistent connection or "login session" for the API. Every time a request is made to `/table/<name>`, the server:
1. Locates the table metadata.
2. Reads the data blocks from the disk.
3. Serializes the records into JSON.
4. Returns the response and closes the task.
