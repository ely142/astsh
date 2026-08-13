# astsh: AST-Based Command Shell (POSIX APIs)

![Language](https://img.shields.io/badge/Language-C%20(gnu17)-blue.svg)
![Build](https://img.shields.io/badge/Build-Make-brightgreen.svg)
![API](https://img.shields.io/badge/API-POSIX-lightgrey.svg)
![Valgrind](https://img.shields.io/badge/Valgrind-Zero--Leak-success.svg)

**astsh** is a modular, UNIX-style shell interpreter. Engineered around a strict **Lexer ➔ Parser ➔ Executor** pipeline, the shell transforms raw input via an FSM-based lexer into an Abstract Syntax Tree (AST) for deterministic evaluation. It handles multi-process lifecycles, n-degree pipeline execution, and standard I/O redirection utilizing standard POSIX system calls. The environment integrates asynchronous job control, signal routing, and state-aware built-ins for directory navigation, command history, and process monitoring.

<div align="center">
  <img src="./assets/demo.gif" alt="astsh execution demonstration" width="100%">
</div>

## 🛠️ Tech Stack

*   **Language:** C (`std=gnu17`)
*   **Compiler & Build:** GCC, GNU Make
*   **Environment:** Native POSIX (Linux, WSL2, macOS)
*   **System APIs:** POSIX standard interfaces (`fork`, `execvp`, `pipe`, `dup2`, `waitpid`, `sigaction`)
*   **Memory Profiling:** Valgrind
*   **Testing & Debugging:** GDB, POSIX Shell (Integration Testing)
*   **Terminal UI:** ANSI Escape Codes & OSC Sequences
