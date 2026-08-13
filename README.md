# astsh | AST-Based Command Shell

![Language](https://img.shields.io/badge/Language-C%20(gnu17)-blue.svg)
![Build](https://img.shields.io/badge/Build-Make-brightgreen.svg)
![API](https://img.shields.io/badge/API-POSIX-lightgrey.svg)
![Valgrind](https://img.shields.io/badge/Valgrind-Zero--Leak-success.svg)

**astsh** is a modular, UNIX-style shell interpreter. Engineered around a strict **Lexer ➔ Parser ➔ Executor** pipeline, the shell transforms raw input via an FSM-based lexer into an Abstract Syntax Tree (AST) for deterministic evaluation. It handles multi-process lifecycles, n-degree pipeline execution, and standard I/O redirection utilizing standard POSIX system calls. The environment integrates asynchronous job control, signal routing, and state-aware built-ins for directory navigation, command history, and process monitoring.

<div align="center">
  <img src="./assets/demo.gif" alt="astsh execution demonstration" width="100%">
</div>

## ✨ Core Features

### Pipeline & I/O Orchestration

* **n-Degree Pipelines (`|`):** Supports arbitrarily long command chaining with properly managed file descriptors to prevent resource exhaustion.  
* **I/O Redirection (`>`, `<`):** Routes stdin and stdout through standard file descriptor duplication.

### Process & Job Management

* **Asynchronous Execution (`&`):** Forks processes to the background without blocking the main shell execution loop.
* **Process Table (`procs`):** Tracks background jobs in real-time, rendering a structural table containing PID, status, and the original command string.
* **Process Signaling (`halt`, `wakeup`, `ice`):** Direct POSIX APIs integration for sending execution state signals and custom kill sequences to active background jobs.
  * `halt` - Suspends execution via `SIGTSTP`.
  * `wakeup` - Resumes suspended jobs via `SIGCONT`.
  * `ice` - Executes `SIGTERM` + `SIGCONT` to flush pending terminations on halted processes.
* **Kernel Signal Reaping:** Implements asynchronous child process cleanup via `SIGCHLD` and `sigaction` with `SA_RESTART` to prevent zombie processes.

### History Expansion & State Mutating

* **In-Memory History (`hist`):** Ring-buffer tracking of the latest 20 used commands.
* **Bang Expansions:** Intercepts raw input for deterministic history expansion, supporting absolute execution (`!!`) and index-based execution (`!n`).
* **Stateful Built-ins (`cd`, `quit`/`exit`):** Routes directory traversal natively to mutate the parent process state and intercepts exit commands to safely tear down the shell, bypassing unnecessary `fork()` overhead.

### Interface & Signal Safety
* **Context Dashboard:** A dynamic, multi-line terminal prompt written with `ANSI` escape codes, featuring path truncation for the user's home directory (`╭─[user@host]─[~/current/path]`).
* **OSC Window Titling:** Dynamically update the host terminal window title.
* **Defensive Signal Routing:** Wraps input polling to gracefully handle `EINTR` interrupts and traps `EOF` (`Ctrl`+`D`) for safe teardown.
* **Zero-Leak Architecture:** Enforces strict memory contracts where the parser/lexer frees tokens and AST nodes immediately per execution cycle, validated via Valgrind.

## 🛠️ Tech Stack

*   **Language:** C (`std=gnu17`)
*   **Compiler & Build:** GCC, GNU Make
*   **Environment:** Native POSIX (Linux, WSL2, macOS)
*   **System APIs:** POSIX standard interfaces (`fork`, `execvp`, `pipe`, `dup2`, `waitpid`, `sigaction`)
*   **Memory Profiling:** Valgrind
*   **Testing & Debugging:** GDB, POSIX Shell (Integration Testing)
*   **Terminal UI:** ANSI Escape Codes & OSC Sequences

## 🚀 Getting Started

### Prerequisites
Ensure the target environment is a UNIX-like operating system (Linux, macOS, or WSL) that supports POSIX API and has the following toolchain installed:

* **GCC** (configured for `-std=gnu17`)  
* **GNU Make**  
* **Valgrind** (strictly required for executing memory validation targets) 

### Compilation & Execution
Clone the repository and compile the source code via the provided `Makefile`:

```bash
git clone https://github.com/ely142/astsh.git
cd astsh
make
```

Execute the compiled binary from the build directory to initialize the shell process:
```bash
./build/astsh
```

To execute the shell with internal additional debugging enabled, pass the `-d` flag:
```bash
./build/astsh -d
```

## 🧪 Testing & Memory Validation

The build system provides dedicated `Makefile` targets to validate core subsystems and enforce the zero-leak memory mandate.

### Component Unit Tests
Compile and execute the standalone test binaries for the Lexer, Parser, and Executor pipelines:
```bash
make test_lexer
make test_parser
make test_executor
```

### End-to-End Integration
Execute the automated test script to validate overall shell functionality: 

```bash
./tests/test_ast.sh
```

The suite performs end-to-end verification of REPL edge cases, pipelines, background job signaling, and I/O redirection, culminating in a final Valgrind memory audit.

### Memory Validation (Valgrind)
The shell architecture enforces a strict zero-leak policy. The `Makefile` natively wraps the execution targets with Valgrind, configured with `--leak-check=full`, `--show-leak-kinds=all`, and `--track-origins=yes` for rigorous memory profiling.

Execute the Valgrind targets to verify memory safety across isolated components or the live shell:

```bash
make valgrind_lexer
make valgrind_parser
make valgrind_executor
make valgrind_shell
```