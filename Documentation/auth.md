# MiniDB Authentication & Security

This document provides an in-depth explanation of the authentication and session management system implemented in MiniDB.

---

## 🔐 Core Security Principles

MiniDB uses a "Security-First" approach to manage access to the database. The system is built on three pillars:
1.  **Identity**: Every connection must be associated with a registered user.
2.  **Confidentiality**: Passwords are never stored in plain text.
3.  **Isolation**: Internal database metadata (like user credentials) is stored separately from user data.

---

## 🛠️ Implementation Details

### 1. Password Hashing (SHA-256)
MiniDB implements a standalone **SHA-256** hashing algorithm. 
- **Mechanism**: When a user is created or logs in, their plain-text password is run through the SHA-256 algorithm to produce a 64-character hexadecimal "fingerprint."
- **Security**: Even if the database files are stolen, the actual passwords remain unknown because SHA-256 is a one-way function.

### 2. Internal Storage (The `system/` folder)
To prevent accidental modification or exposure, MiniDB uses a dual-directory structure:
- `table/`: Stores user-created tables and data.
- `system/`: Stores internal engine tables (like the `auth` table).
  - `system/auth/met`: Metadata for the authentication table.
  - `system/auth/data.dat`: The actual records containing usernames and password hashes.

### 3. The Auth Table Schema
The internal `auth` table is managed by the storage engine itself and follows this schema:
| Column | Type | Size | Description |
| :--- | :--- | :--- | :--- |
| `id` | INT | 4 | Primary Key (auto-incremented) |
| `username` | VARCHAR | 50 | Unique identifier for the user |
| `password_hash` | VARCHAR | 64 | The SHA-256 hash of the password |

---

## 🖥️ CLI Authentication Flow

### First-Run Experience
If MiniDB detects that no users exist in the `system/` directory:
1.  It enters **Setup Mode**.
2.  It prompts the user to set a password for the username provided via the `-u` flag.
3.  The password input is masked (hidden) using `termios` for security.
4.  Once confirmed, the user is registered as the initial administrator.

### Standard Login
For subsequent runs:
1.  The system prompts for the password of the specified user.
2.  It hashes the input and compares it against the hash stored in `system/auth/data.dat`.
3.  If they match, access is granted to the Query Console.

---

## 📡 API Security & Session Management

The REST API is stateless by default but uses **Token-Based Authentication** to maintain secure sessions.

### 1. Authentication Flow
- **Login**: A `POST` request to `/login` with JSON credentials.
- **Verification**: The server verifies the hash.
- **Token Generation**: Upon success, the server generates a cryptographically random 32-character **Session Token**.
- **Storage**: This token is stored in an in-memory map on the server, linked to the username.

### 2. Authorization
For all other endpoints (e.g., `/tables`, `/query`), the client must provide the token in the HTTP header:
`X-Session-Token: <your_token_here>`

If the token is missing, expired, or invalid, the server returns a `401 Unauthorized` response.

### 3. Session Termination
- **Logout**: A `POST` request to `/logout` with the token will remove the token from the server's memory, effectively ending the session.
- **Restart**: Since tokens are stored in-memory, restarting the API server clears all active sessions for security.

---

## 🚀 Future Roadmap
The current authentication system provides the foundation for:
- **RBAC (Role-Based Access Control)**: Assigning specific permissions (READ, WRITE, DROP) to different users.
- **Private Tables**: Restricting table visibility so users can only see the data they created.
- **Audit Logging**: Tracking which user performed which action on the database.
