# Session: 2026-07-06 — Initial Codebase Review & Study Setup

## Goal
Set up the study infrastructure for the dns-proxy-cache project: read every source file, understand the architecture, create the code map, identify review seeds, and establish the session workflow for ongoing code review.

## Phase: Pre-Work
_Full codebase read-through and note collection._

- [x] Read all source files: main.c, proxy.c, dns_wire.c, dns_wire.h, proxy.h, patch.h, wsa_init_helper.c
- [x] Read build config: CMakeLists.txt
- [x] Read project docs: README.md, git-commit.md
- [x] Read test client: dns_test.py
- [x] Read BUILD DNS vault notes: roadmap, journey, M2 mistakes, M1→M2 transition, epoll explainer

### Target Files
All files in the repository — this was a full-codebase orientation session.

### Key Questions Going In
- What is the overall architecture of the proxy?
- How does the event loop work?
- What state tracking exists for pending requests?
- Where are the most likely bugs?
- What's the gap between M2 (done) and M3 (planned)?

## Phase: Code Review

### What I Read

| File | Lines | Time Spent |
|---|---|---|
| main.c | 1–133 (complete) | M1 entry point, single-shot resolver |
| proxy.c | 1–273 (complete) | M2 proxy with epoll event loop |
| dns_wire.c | 1–134 (complete) | Wire format encoding/decoding |
| dns_wire.h | 1–19 (complete) | Public API declarations |
| proxy.h | 1–10 (complete) | Stale/minimal header |
| patch.h | 1–11 (complete) | Debug shim for Windows |
| wsa_init_helper.c | 1–35 (complete) | WSA auto-init with GCC constructors |
| CMakeLists.txt | 1–57 (complete) | Build configuration |

### Findings

| Finding | File:Line | Severity | Notes |
|---|---|---|---|
| Transaction ID collision | proxy.c:54-61 | 🔴 Bug | `find()` returns first match; duplicate IDs cause wrong routing |
| No request timeout | proxy.c (global) | 🔴 Leak | Unanswered queries stay in linked list forever |
| sendto return value ignored | proxy.c:218,256 | 🟡 Design | Failed sends leave orphan nodes or silent drops |
| add() return value ignored | proxy.c:216 | 🟡 Design | Failed alloc → query forwarded but untracked |
| 1024-byte buffer limit | proxy.c:196,223 | 🟡 Design | EDNS0 responses can exceed this, causing silent truncation |
| Hardcoded ID 0xAAAA | dns_wire.c:43 | 🟡 Latent | Not a problem in M2 (not called) but dangerous if reused |
| perror on non-errno error | proxy.c:239 | 🔵 Minor | "Dropped packet" message prints misleading errno text |
| proxy.h is stale | proxy.h:8 | 🔵 Cleanup | Declares socket() — conflicts with system headers |
| Windows proxy is no-op | proxy.c:261-264 | 🔵 Known | Sleep(1000) loop, no actual I/O |

### Trace Walkthrough: Client Query → Response

```
1. Client sends DNS query to 127.0.0.1:5353
2. epoll_wait() wakes up, reports local_socket readable
3. recvfrom(local_socket) → buffer + client_addr
4. Extract ID from buffer[0:2]
5. add(id, &client_addr, len, buffer, size) → prepend to linked list
6. sendto(upstream_sock, buffer, size, ..., 8.8.8.8:53) → forward raw bytes
7. ... time passes ...
8. epoll_wait() wakes up, reports upstream_sock readable
9. recvfrom(upstream_sock) → response_buffer + google_addr
10. Validate sender: google_addr == 8.8.8.8:53
11. Extract ID from response_buffer[0:2]
12. find(id) → search linked list for matching node
13. sendto(local_socket, response_buffer, size, ..., node->client_addr) → reply to client
14. delete(id) → free the node
```

## Phase: Experimentation
_No code changes or test runs this session — focus was reading and documenting._

## Phase: Reflection

- [x] Created docs/CODE_MAP.md — function-by-function map
- [x] Created docs/REVIEW-SEEDS.md — 14 specific review seeds
- [x] Created docs/PROGRESS.md — milestone tracker + known unknowns
- [x] Created sessions/TEMPLATE.md — adapted for code review workflow

### Key Takeaway
The proxy's core loop is clean and correct for the happy path, but it has no defense against time — requests that never get answered accumulate forever, and ID collisions have no mitigation.

### Concepts Clarified
- Why the proxy needs two sockets (separation of client-facing and upstream roles)
- Why `bind()` is needed for local but not upstream (ephemeral port assignment)
- How `epoll_wait` + drain loops achieve non-blocking I/O
- Why `select()` was right for M1 (single fd, one-shot) but wrong for M2

### Still Fuzzy
- DNS compression pointers (0xC0) — how they work in answer sections
- EDNS0 OPT pseudo-records — format and implications for buffer sizing
- Whether level-triggered epoll with drain loops is the right pattern or an edge-triggered habit
- Exact memory layout of `epoll_event` struct and `data.fd` vs `data.ptr`

### Next Session Plan
Deep-dive into Review Seed #1 (Transaction ID collision). Trace through what actually happens with two concurrent clients using the same ID. Consider whether the fix belongs in M2 (now) or M3 (with the cache redesign). Possibly prototype an ID-remapping approach.
