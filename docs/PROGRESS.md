# PROGRESS — dns-proxy-cache

> Current state of the project and learning journey.
> Last updated: 2026-07-06

---

## Milestone Status

| Milestone | Status | Description | Key Files |
|---|---|---|---|
| **M1** | ✅ Done | Single-shot DNS resolver — build query, send to 8.8.8.8, parse reply | `main.c`, `dns_wire.c`, `dns_wire.h` |
| **M2** | ✅ Done | epoll-based UDP proxy — dual-socket, linked-list tracking, non-blocking I/O | `proxy.c` |
| **M3** | 📋 Planned | TTL-aware DNS cache | `dns_cache.c` (doesn't exist yet, CMakeLists.txt ready for it) |
| **M4** | 📋 Planned | TCP fallback with buffering | — |
| **M5** | 📋 Planned | Polish, stats, leak-free cleanup | — |

---

## What M1 Proved

- [x] DNS wire format encoding (label-prefixed names)
- [x] Building a valid A-record query packet from scratch
- [x] Sending over UDP to a real resolver (8.8.8.8)
- [x] `select()` for single-fd timeout waiting
- [x] Parsing response: skipping header + question, extracting A-record IP
- [x] Memory ownership: caller frees heap-allocated query buffer
- [x] Cross-platform socket abstraction (Windows/Linux macros)

## What M2 Proved

- [x] Dual-socket architecture (local listener + upstream client)
- [x] `bind()` for the local socket, ephemeral port for upstream
- [x] Non-blocking sockets via `fcntl(O_NONBLOCK)`
- [x] `epoll` lifecycle: `epoll_create1` → `epoll_ctl(ADD)` → `epoll_wait` loop
- [x] Event-driven drain loops (read until `EAGAIN`)
- [x] Linked-list state tracking keyed by transaction ID
- [x] Upstream sender validation (IP + port check)
- [x] Minimum packet length validation (≥ 2 bytes for ID extraction)

---

## Key Concepts Already Studied (from BUILD DNS notes)

### Concurrency & OS Fundamentals
- [x] Process internals (`task_struct`, PID, memory layout)
- [x] Signals (pending/blocked bitmaps, delivery lifecycle)
- [x] Threads vs processes (`clone()` vs `fork()`, shared vs private resources)

### I/O Foundations
- [x] File descriptor tables (per-process, system-wide, inode table)
- [x] `dup2()`, reference counting

### I/O Multiplexing Progression
- [x] `select()` — function signature, fd_set memory layout, O(N) scanning
- [x] Why `select()` is wrong for a DNS proxy (FD_SETSIZE=1024, linear scan, stateless)
- [x] `poll()` — struct pollfd, no FD_SETSIZE limit, still O(N)
- [x] `epoll` — persistent kernel state, interest list (red-black tree), ready list, O(events) not O(fds)
- [x] Why `epoll` is superior: kernel callbacks, no per-call fd copying, wake-on-event

### Protocol Understanding
- [x] DNS is a message protocol, not a function call
- [x] Transaction ID is the correlation key
- [x] UDP is connectionless — must store client identity for reply routing
- [x] `recvfrom()` returns both data AND sender identity

### Architecture Lessons Learned (from M2 mistakes doc)
- [x] Socket endpoint mismatch (sending on wrong socket)
- [x] Destination address confusion (replying to self)
- [x] `sizeof(buffer)` vs actual received bytes
- [x] `sockaddr` type/size inconsistency
- [x] Array pointer decay (`buffer` vs `&buffer`)
- [x] Zero ID bug (ID=0 is valid per RFC 1035)
- [x] Redundant state lookup (response ID == request ID)
- [x] Blind payload parsing (bounds check before ID extraction)
- [x] UDP origin spoofing vulnerability

---

## Known Unknowns

### Technical — Must Answer Before M3
- [ ] How to extract TTL from arbitrary DNS responses (not just simple A records)
- [ ] Cache eviction strategy (LRU? TTL-based expiry? both?)
- [ ] Should the cache key be `(domain, QTYPE, QCLASS)` or just `domain`?
- [ ] How to handle CNAME chains in cached responses
- [ ] Storage backend: in-memory hash table? SQLite? (journey.md mentions SQL/Redis ideas)

### Technical — Must Answer Before M4
- [ ] TCP DNS framing (2-byte length prefix)
- [ ] When to fall back to TCP (TC flag in response, or query > 512 bytes)
- [ ] Connection management for TCP upstream (persistent connections? connection pooling?)
- [ ] How `epoll` handles TCP sockets differently from UDP

### Design Questions
- [ ] Transaction ID collision handling (see REVIEW-SEEDS.md #1)
- [ ] Request timeout/eviction for the linked list (no current mechanism)
- [ ] Whether to use custom malloc (mentioned in journey.md as an idea)
- [ ] Graceful shutdown and signal handling
- [ ] Windows support strategy (WSL only? or real Windows I/O?)

### Knowledge Gaps to Fill
- [ ] DNS compression pointers (RFC 1035 §4.1.4) — currently rejected
- [ ] EDNS0 (RFC 6891) — larger UDP payloads, OPT pseudo-records
- [ ] DNSSEC basics — affects response sizes significantly
- [ ] `epoll` edge-triggered mode (`EPOLLET`) — when and why to use it
- [ ] Real-world DNS proxy architectures (dnsmasq, Unbound, CoreDNS)

---

## Study Resources Used

| Source | Location | Content |
|---|---|---|
| 00 Home & Roadmap.md | BUILD DNS vault | Central hub linking all notes |
| journey.md | BUILD DNS vault | Ideas for M3 (SQL cache, custom malloc) |
| our mistakes while buiding M2.md | BUILD DNS vault | 9 categorized architectural mistakes |
| the moving from M1 to M2.md | BUILD DNS vault | Design rationale for dual-socket, epoll, linked list |
| wtf is epoll.md | BUILD DNS vault | epoll mental model: create → ctl → wait, interest/ready lists |
| 03-03 why select() is wrong.md | BUILD DNS vault | FD_SETSIZE limits, performance analysis |
| M2 questions.md | BUILD DNS vault | Deep Q&A on M2 implementation |
| 07 Questions & Answers.md | BUILD DNS vault | 27 technical questions covering full vault |
