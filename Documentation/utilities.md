# Utilities

This document describes the custom input handling and command history features implemented in MiniDB.

## Keyboard Shortcuts

The Query Console uses a custom `HistoryManager` to provide an enhanced terminal experience. Below are the supported keyboard shortcuts:

| Shortcut | Action |
| :--- | :--- |
| **Up Arrow** | Navigate to the previous command in history. |
| **Down Arrow** | Navigate to the next command in history (or back to the current draft). |
| **Left Arrow** | Move the cursor one character to the left. |
| **Right Arrow** | Move the cursor one character to the right. |
| **Home (Fn + Left)** | Move the cursor to the beginning of the line. |
| **End (Fn + Right)** | Move the cursor to the end of the line. |
| **Ctrl + Left** | Skip one word to the left. |
| **Ctrl + Right** | Skip one word to the right. |
| **Backspace** | Delete the character to the left of the cursor. |
| **Delete** | Delete the character at the cursor position. |
| **Ctrl + Backspace** | Delete the entire word to the left of the cursor. |
| **Alt + Backspace** | Clears the entire command. |
| **Ctrl + C** | Interrupt the current process (standard signal handling). |
| **Ctrl + D** | Exit the Query Console (if the line is empty). |

## Features

- **Command History**: All executed queries are stored in a session-based queue.
- **In-line Editing**: You can move the cursor anywhere in your current command to make precise tweaks without deleting the entire line.
- **Draft Preservation**: If you start typing a command and then use the Up arrow to check history, your original unfinished command is saved as a "draft" and can be retrieved by pressing the Down arrow until you return to the end of the history.
- **Raw Mode handling**: The terminal is automatically switched to raw mode during input to capture special key sequences and switched back immediately after.
