# Contributing to dns-proxy-cache

Thank you for your interest in contributing to the **dns-proxy-cache** project! This document outlines the standards, workflow, and expectations for contributors.

---

## Code of Conduct & Development Principles

We build lightweight, highly performant systems from scratch using radical clarity. When writing code:
1. **Understand the Packet:** Keep wire-format encoding/decoding distinct from networking flow.
2. **Own the Memory:** Every `malloc` must have a matching `free`. Run memory leak checks before submitting changes.
3. **No Placeholders:** Write production-ready code with robust boundary checks.

---

## Coding Standards

- **Language Standard:** C11 (strict warning options enabled).
- **Format:** Maintain consistent indentation and styling.
- **Portability:**
  - Code should compile on both Linux and Windows.
  - Sockets must use appropriate cross-platform macros (e.g. `CLOSESOCKET`, `SOCKET` typedef, `WSAStartup`).
  - Note: The UDP proxy currently uses `epoll` for event multiplexing on Linux and falls back to a sleep loop on Windows. If implementing I/O multiplexing, ensure Linux utilizes `epoll`.

---

## Git & Commit Guidelines

We enforce the [Conventional Commits](https://www.conventionalcommits.org/) specification for all commits:

- `feat(...)`: A new feature (e.g., caching, TCP support)
- `fix(...)`: A bug fix
- `docs(...)`: Documentation changes
- `refactor(...)`: Code changes that neither fix a bug nor add a feature
- `test(...)`: Adding missing tests or correcting existing tests
- `chore(...)`: General repository maintenance or build system updates

### Branching Strategy
- Branch from `main` or the current active development branch (`moving-to-epoll`).
- Name branches descriptively (e.g., `feat/udp-dns-cache`, `fix/pointer-bounds-check`).
- Open a Pull Request (PR) against the target development branch.

---

## How to Build and Test

### Prerequisites
- CMake 3.18 or higher
- C compiler (GCC, Clang, or MSVC)
- Python 3.x (for running test scripts)

### Building
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

This builds two executables in the `build/` directory:
- `dns_query`: A single-shot DNS resolver.
- `dns_proxy`: The UDP DNS proxy server.

### Testing
To test the proxy locally:
1. Start the proxy server:
   ```bash
   ./build/dns_proxy
   ```
   *(By default, it listens on `127.0.0.1:5353` and forwards queries to `8.8.8.8`)*

2. In a separate terminal, run the Python test suite:
   ```bash
   python dns_test.py
   ```

---

## Pull Request Process

1. Ensure the code builds cleanly with zero compiler warnings.
2. Verify that memory is correctly managed and free of leaks.
3. Keep PRs focused on a single logical change.
4. Fill out the pull request template completely.
