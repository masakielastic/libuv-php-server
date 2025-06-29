# Aurora Server: Refactoring and Feature Suggestions

This document outlines potential refactoring and feature improvements to enhance the Aurora server's quality, performance, and developer experience.

## 1. Refactoring (Improving Internal Quality)

These suggestions focus on making the current foundation more robust and maintainable.

### 1.1. Enhanced Error Handling

-   **Current State**: Errors within `libaurora` (e.g., failed to load certificate) are printed directly to `stderr` via `fprintf`.
-   **Problem**: The library's consumer (`aurora.c`) cannot programmatically detect these errors to take appropriate action (e.g., issue a PHP warning, halt execution).
-   **Proposal**: Modify functions like `http_server_create` to return an integer status code. Detailed error messages can be returned via an output parameter (e.g., `const char** error_reason`). This would allow `aurora.c` to catch C-level errors and bubble them up as proper PHP `E_WARNING` or `E_ERROR` notifications.

### 1.2. HTTP Keep-Alive Support

-   **Current State**: The server closes the connection immediately after sending a response.
-   **Problem**: This prevents HTTP Keep-Alive, forcing a new TCP and TLS handshake for every single request, which is highly inefficient.
-   **Proposal**:
    1.  Parse the `Connection` header. By default, keep the connection open unless `Connection: close` is specified.
    2.  Remove the `uv_close` call from the final write callback (`on_final_write_cb`).
    3.  Implement a timeout mechanism using `uv_timer_t` to automatically close idle connections from the server side.

### 1.3. Stricter Memory Management

-   **Current State**: `malloc`/`free` and `strdup` are used frequently, creating a risk of memory leaks, especially in error paths.
-   **Problem**: Memory leaks are critical bugs for a long-running server process.
-   **Proposal**: In `aurora.c`, use PHP's memory manager (`emalloc`/`efree`) for request-bound allocations. Within `libaurora`, create matching `_create`/`_destroy` functions for all structs to ensure resources are consistently managed.

## 2. Feature Improvements (A Better Development Server)

New features to improve the developer experience.

### 2.1. Multi-Process (Worker) Model

-   **Current State**: The server operates on a single process and single thread.
-   **Problem**: It cannot leverage multi-core CPUs, limiting concurrent request handling to one.
-   **Proposal**: When the `aurora` command starts, `fork()` a number of worker processes corresponding to the CPU core count. The parent process would monitor the workers, and each worker would run its own `libuv` event loop to handle requests in parallel.

### 2.2. Hot-Reloading

-   **Current State**: PHP files must be manually reloaded by restarting the server.
-   **Problem**: This manual step slows down the development cycle.
-   **Proposal**: Use `libuv`'s filesystem event API (`uv_fs_event_t`) to monitor the document root for changes to `.php` files. When a change is detected, the parent process can signal the workers (e.g., via `SIGHUP`) to gracefully restart, making changes immediately available upon saving a file.

### 2.3. Advanced Logging

-   **Current State**: Logging is limited to `printf` calls to standard output.
-   **Problem**: No structured access logging or detailed error recording is available.
-   **Proposal**: Add command-line options to control log level (e.g., `--log-level=debug`) and specify an access log destination. The main request handler would be responsible for formatting and writing structured log entries.

## Recommended Roadmap

1.  **Implement Keep-Alive Support (1.2)**: This will provide the most significant and immediate performance improvement.
2.  **Implement Hot-Reloading (2.2)**: This will provide the most significant improvement to the development workflow.
3.  **Enhance Error Handling (1.1)**: This will make the server more stable and predictable for users.
