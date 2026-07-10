# CODE MAP — dns-proxy-cache

> Function-by-function map of every file in the project.
> Last updated: 2026-07-06

---

## dns_wire.h

Minimal public header. Declares three functions and includes `<stddef.h>` + `<stdint.h>`.

> [!NOTE]
> This header is a known gap — it contains zero documentation and no struct definitions.
> All DNS wire-format knowledge is implicit in `dns_wire.c`.

### Declarations

| Function | Signature |
|---|---|
| `build_dns_query` | `char *build_dns_query(const char *hostname, size_t *out_len)` |
| `is_dns_reply_valid` | `int is_dns_reply_valid(const unsigned char *buffer, size_t buffer_len, uint16_t sent_id)` |
| `pars_reply` | `char *pars_reply(const unsigned char *buffer, size_t buffer_len, uint16_t sent_id, char *out_buffer, size_t out_len)` |

---

## dns_wire.c

DNS wire-format encoding and decoding. Used by M1 (`main.c`) directly; **not** used by M2 (`proxy.c`) at runtime — the proxy forwards raw bytes.

### encode_dns_name()
- **Purpose:** Converts a dotted hostname (`example.com`) into DNS wire label format (`\x07example\x03com\x00`).
- **Key details:** Static function. Iterates character-by-character, writing length-prefixed labels. Rejects empty labels and labels > 63 bytes. Does **not** bounds-check the output buffer — relies on the caller (`build_dns_query`) allocating 512 bytes.
- **Calls:** `memcpy`
- **Called by:** `build_dns_query()`

### build_dns_query()
- **Purpose:** Allocates a 512-byte buffer and builds a complete DNS A-record query packet.
- **Key details:** Hardcodes transaction ID `0xAAAA`. Sets flags `0x0100` (standard query, recursion desired). Writes 12-byte header, encoded question name, QTYPE=A (`0x0001`), QCLASS=IN (`0x0001`). Returns heap-allocated buffer — **caller owns it**.
- **Calls:** `malloc`, `free`, `encode_dns_name()`
- **Called by:** `main()` in `main.c`

### is_dns_reply_valid()
- **Purpose:** Validates a DNS response buffer against basic structural checks.
- **Key details:** Checks: buffer ≥ 12 bytes, QR bit set (byte 2 bit 7), transaction ID match, RCODE == 0 (no error), ANCOUNT > 0. Returns 1 (valid) or 0 (invalid).
- **Calls:** nothing
- **Called by:** `pars_reply()`

### pars_reply()
- **Purpose:** Extracts the first A-record IPv4 address from a DNS response.
- **Key details:** Calls `is_dns_reply_valid()` first. Skips the 12-byte header, walks the question section label-by-label, skips the null terminator + QTYPE/QCLASS (5 bytes), then reads the first answer record. Expects RDLENGTH == 4 (IPv4). Formats the IP with `snprintf`. Rejects compressed names (`0xC0` pointer) in the question section but does **not** handle them in the answer section.
- **Calls:** `is_dns_reply_valid()`, `snprintf`
- **Called by:** `main()` in `main.c`

---

## main.c (M1 — Single-shot resolver)

Entry point for the `dns_query` executable. Demonstrates M1: build a query, send it to `8.8.8.8`, wait with `select()`, parse the reply.

### main()
- **Purpose:** End-to-end single DNS query and response cycle.
- **Key details:**
  1. WSAStartup on Windows
  2. Builds query via `build_dns_query("example.com")`
  3. Extracts the sent transaction ID from bytes 0–1
  4. Creates a UDP socket, sends query to `8.8.8.8:53`
  5. Waits with `select()` (3-second timeout)
  6. Receives response into 1024-byte stack buffer
  7. Parses via `pars_reply()`, prints IP
  8. Frees heap query, closes socket on every exit path
- **Calls:** `build_dns_query()`, `pars_reply()`, `socket`, `sendto`, `select`, `recvfrom`, `inet_pton`, `free`, `CLOSESOCKET`
- **Called by:** OS (entry point)

### Cross-platform details
- `CLOSESOCKET` macro: `closesocket()` on Windows, `close()` on Linux
- `SOCKET` typedef: Windows uses `SOCKET` type, Linux uses `int`
- `select()` nfds: 0 on Windows (ignored), `sock + 1` on Linux

---

## proxy.c (M2 — epoll-based proxy)

The main proxy executable. Listens on `127.0.0.1:5353`, forwards queries to `8.8.8.8:53`, routes responses back using a linked list keyed by transaction ID. Uses `epoll` on Linux; Windows path is a no-op sleep loop.

### request_node (struct Node)
- **Purpose:** Tracks a pending client request.
- **Fields:**
  - `ID` — 16-bit DNS transaction ID
  - `client_addr` — client's `sockaddr_in` (IP + port)
  - `addr_len` — `socklen_t` for the address
  - `request_Buffer` — heap copy of the raw query bytes
  - `request_size` — byte count of the query
  - `next` — singly-linked list pointer

### Head (global)
- **Purpose:** Global head pointer for the pending-request linked list.
- **Type:** `request_node *`, initialized to `NULL`.

### add()
- **Purpose:** Prepends a new request node to the linked list.
- **Key details:** Validates inputs (null check, zero-size check). `malloc`s the node + a separate `malloc` for the buffer copy. Inserts at head (O(1)). Returns 0 on success, -1 on failure.
- **Calls:** `malloc`, `memcpy`, `fprintf`
- **Called by:** event loop (local_socket branch)

### find()
- **Purpose:** Linear search for a node by transaction ID.
- **Key details:** O(N) walk from Head. Returns first match or NULL.
- **Calls:** nothing
- **Called by:** event loop (upstream_sock branch)

### delete()
- **Purpose:** Removes and frees a node by transaction ID.
- **Key details:** Handles head-removal case separately. Frees both `request_Buffer` and the node itself. Only deletes the **first** match.
- **Calls:** `free`
- **Called by:** event loop (upstream_sock branch, after `find()`)

### main() — proxy entry point
- **Purpose:** Sets up dual sockets and the epoll event loop.
- **Key details:**
  1. **Socket setup:** Creates `local_socket` (bound to `127.0.0.1:5353`) and `upstream_sock` (unbound, ephemeral port)
  2. **Non-blocking:** Uses `fcntl()` with `O_NONBLOCK` on both sockets (Linux only)
  3. **epoll setup:** `epoll_create1(0)`, registers both sockets with `EPOLLIN`
  4. **Event loop:**
     - `epoll_wait()` blocks until events arrive (timeout = -1, infinite)
     - **local_socket ready:** inner `while(1)` drains all pending client queries (handles `EAGAIN`). For each: extracts ID from bytes 0–1, calls `add()`, forwards raw bytes to upstream via `sendto()`
     - **upstream_sock ready:** inner `while(1)` drains all upstream responses. Validates sender IP/port against `8.8.8.8:53`. Extracts ID, calls `find()`, sends response back to client via `sendto()`, calls `delete()`
  5. **Windows fallback:** `Sleep(1000)` in a loop (no actual I/O handling)
- **Calls:** `socket`, `bind`, `inet_pton`, `fcntl`, `epoll_create1`, `epoll_ctl`, `epoll_wait`, `recvfrom`, `sendto`, `add()`, `find()`, `delete()`, `closesocket`, `close`
- **Called by:** OS (entry point)

---

## patch.h

Debug/testing shim for Windows cross-compilation. **Not included by any production source file** (appears to be a manual experiment).

### Contents
- Redefines `htons(x)` macro — remaps port 5353 → 5354 (testing hack)
- Declares `my_socket_wrapper()` and `#define socket my_socket_wrapper` — intercepts all `socket()` calls
- **Key risk:** The `htons` macro redefines a system function. If included, it silently changes binding behavior.

---

## wsa_init_helper.c

Windows-specific helper that auto-initializes Winsock via GCC constructor attributes. Linked only on Windows builds.

### init_winsock()
- **Purpose:** GCC `__attribute__((constructor))` — runs before `main()`. Calls `WSAStartup` and disables stdout/stderr buffering.
- **Key details:** Uses `setvbuf` to force unbuffered output for debugging. Prints diagnostic messages.
- **Calls:** `WSAStartup`, `setvbuf`, `printf`
- **Called by:** C runtime (automatic, before `main()`)

### cleanup_winsock()
- **Purpose:** GCC `__attribute__((destructor))` — runs after `main()`. Calls `WSACleanup`.
- **Calls:** `WSACleanup`, `printf`
- **Called by:** C runtime (automatic, after `main()`)

### my_socket_wrapper()
- **Purpose:** Wraps the real `socket()` call with diagnostic logging.
- **Key details:** `#undef socket` at top of file to access the real Winsock `socket()`. Prints the fd value and checks `INVALID_SOCKET`. Returns `uintptr_t` (to handle Windows SOCKET type portably).
- **Calls:** `socket` (real), `printf`, `fprintf`, `WSAGetLastError`
- **Called by:** any code that includes `patch.h` (via `#define socket my_socket_wrapper`)

---

## CMakeLists.txt

### Build targets

| Target | Type | Sources | Links |
|---|---|---|---|
| `dns_core` | Static library | `dns_wire.c` (+ `dns_cache.c` if it exists) | — |
| `dns_query` | Executable (M1) | `main.c` | `dns_core`, `ws2_32` (Windows) |
| `dns_proxy` | Executable (M2) | `proxy.c` | `dns_core`, `ws2_32` (Windows) |

### Notable details
- C11 standard, extensions off
- Strict warnings: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` (GCC/Clang) or `/W4` (MSVC)
- Conditionally includes `dns_cache.c` if the file exists (M3 prep)
- `dns_proxy` is controlled by `ENABLE_PROXY_TARGET` option

---

## dns_test.py

Python test client. Sends a hand-crafted DNS query (for `google.com`, ID `0x1234`) to `127.0.0.1:5353` and prints the response. Naive IP extraction (last 4 bytes) — works only for simple single-answer responses.

---

## git-commit.md

Conventional Commits style guide for the project. Not code — developer reference.
