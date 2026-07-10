# Phase 1: Known Knowns — Comprehensive Q&A Bank

> **Purpose:** Systematically identify what you KNOW, what you UNDERSTAND, and where your gaps are.
> **Method:** Progressive questioning from Level 0 (project identity) through Level 5 (FAANG deep-dives).
> **Instructions:** Answer each question OUT LOUD or in writing BEFORE reading the answer tips. Mark each as: ✅ Solid | ⚠️ Shaky | ❌ Gap.

---

## Level 0: Project Identity & Problem Statement

> *Before touching a single line of code, can you explain what this project IS and WHY it exists?*

### Q0.1: What is this project?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:** Don't say "it's a DNS thing." A FAANG interviewer wants precision. Practice saying: *"It's a userspace UDP proxy that sits between local clients and an upstream DNS resolver. Clients send standard DNS queries to localhost:5353, the proxy forwards them to Google Public DNS (8.8.8.8:53), receives the responses asynchronously, and routes each response back to the correct originating client."*
> **Depth Upgrade:** Mention that it's a *transparent* proxy — the client doesn't know it's talking to a proxy, not a real DNS server.

---

### Q0.2: What real-world problem does a DNS proxy solve?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:** There are 3 real reasons to proxy DNS:
> 1. **Caching** — avoid repeating the same query to upstream (reduces latency, saves bandwidth).
> 2. **Filtering/Policy** — block certain domains (ad-blocking, parental controls, corporate policy).
> 3. **Logging/Monitoring** — see what domains are being resolved on a network.
>
> Our proxy currently does NONE of these (caching is M3). Right now it's a **pass-through forwarder** — its value is as a *learning vehicle* for systems programming concepts.
> **Depth Upgrade:** Mention real DNS proxies in the wild: `dnsmasq`, `Pi-hole`, `CoreDNS`, Cloudflare's `1.1.1.1` client.

---

### Q0.3: What are the milestones, and what does each one prove?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:**
> | Milestone | What it builds | What it PROVES you understand |
> |---|---|---|
> | **M1** | Single-shot DNS resolver (`main.c`) | UDP sockets, DNS wire format, `select()` I/O multiplexing, byte-level protocol construction |
> | **M2** | Multi-client async proxy (`proxy.c`) | `epoll` event loops, non-blocking I/O, application-layer state management over stateless UDP, linked list data structures in C |
> | **M3** (planned) | TTL-aware caching + TCP fallback | Cache data structures, TTL expiration, protocol switching (UDP→TCP), memory management at scale |
> **Depth Upgrade:** Explain the *progression*: M1 is synchronous (one query, block, one reply). M2 is asynchronous (many queries interleaved, no blocking). M3 adds persistence (state survives across queries).

---

### Q0.4: Why C? Why not Python, Go, or Rust?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:** This is a *systems programming* learning project. C forces you to:
> - Manage memory manually (`malloc`/`free`) — no garbage collector.
> - Work directly with the POSIX socket API (`socket`, `bind`, `sendto`, `recvfrom`).
> - Understand byte-level wire formats (endianness, bitmasking, pointer arithmetic).
> - See what the OS actually does (system calls vs. library abstractions).
> **Depth Upgrade:** In a FAANG interview, say *"I chose C because I wanted to understand the syscall boundary — what the kernel provides vs. what the application must build. In Go, `net.ListenPacket` hides the epoll loop. In C, I built it myself."*

---

## Level 1: Architecture & Component Roles

> *Can you name every file, explain its role, and draw the dependency graph from memory?*

### Q1.1: What are the two executables this project produces, and what does each one do?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:** The `CMakeLists.txt` defines two targets:
> - **`dns_query`** (M1): Links `main.c` + `dns_core` library. Single-shot resolver.
> - **`dns_proxy`** (M2): Links `proxy.c` + `dns_core` library. Multi-client async proxy.
> Both link against `dns_core` (static library from `dns_wire.c`), but **M2 never calls any `dns_wire.c` functions at runtime** — it forwards raw bytes without parsing them.
> **Depth Upgrade:** Explain WHY M2 doesn't parse: *"The proxy is transparent. It doesn't need to understand the DNS payload — it just needs to route it. Parsing would add latency and complexity for zero benefit."*

---

### Q1.2: What does `dns_wire.c` do, and which milestone actually uses it?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:** `dns_wire.c` contains 4 functions:
> 1. `encode_dns_name()` — converts `"example.com"` → `"\x07example\x03com\x00"` (DNS label format)
> 2. `build_dns_query()` — builds a complete 512-byte DNS A-record query packet
> 3. `is_dns_reply_valid()` — validates response header (QR bit, RCODE, ANCOUNT, ID match)
> 4. `pars_reply()` — extracts the first A-record IPv4 address from a response
>
> **Only M1 uses these.** M2 forwards raw client bytes — it never constructs or parses DNS packets itself.
> **Depth Upgrade:** Ask yourself: *"If M2 doesn't parse DNS, how does it know the transaction ID?"* Answer: It reads bytes 0–1 directly from the raw buffer (`ntohs(*(uint16_t*)buffer)`) — it doesn't need the full `dns_wire.c` parsing logic.

---

### Q1.3: What is `proxy.c`'s architecture in one sentence?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:** Practice this exact sentence: *"`proxy.c` is an epoll-driven event loop that multiplexes two non-blocking UDP sockets — one facing clients (port 5353) and one facing upstream (8.8.8.8:53) — and uses a singly-linked list of `request_node` structs to map DNS transaction IDs to client socket addresses for asynchronous response routing."*
> **Depth Upgrade:** Notice every technical word in that sentence carries weight. If you can't explain any one of them (epoll, multiplexes, non-blocking, singly-linked, transaction ID, socket address, asynchronous), that's a Known Unknown.

---

### Q1.4: What is `patch.h` and `wsa_init_helper.c`? Are they used in production?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:**
> - `patch.h`: A debug/testing shim. Redefines `htons()` to remap port 5353→5354 and wraps `socket()` with diagnostic logging. **NOT included by any production source file.** It's leftover scaffolding.
> - `wsa_init_helper.c`: Windows-only. Uses GCC constructor/destructor attributes to auto-call `WSAStartup`/`WSACleanup` before/after `main()`. Also wraps `socket()` for debug logging. Only linked on Windows builds.
> **Depth Upgrade:** Explain GCC `__attribute__((constructor))` — it's a function that runs BEFORE `main()`. The C runtime calls it during `.init` section processing. This is how libraries like `pthread` initialize internal state.

---

## Level 2: Core Concepts & Mechanisms

> *Can you explain HOW things work, not just WHAT they are?*

### Q2.1: What is a UDP socket, and how is it different from a TCP socket?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:**
> | Property | UDP | TCP |
> |---|---|---|
> | Connection state | **Connectionless** — no handshake, no connection tracking | Connection-oriented — 3-way handshake, kernel tracks state |
> | Delivery guarantee | **None** — packets can be lost, duplicated, or reordered | Guaranteed in-order delivery with retransmission |
> | Message boundaries | **Preserved** — each `sendto` = one datagram | Byte stream — no message boundaries |
> | Kernel state per client | **Zero** — one socket serves all clients | One socket (FD) per client connection |
> **Depth Upgrade:** The last row is WHY our proxy needs `request_node`. TCP gives you one FD per client, so the kernel routes replies for you. UDP gives you one shared FD, so YOU must route replies yourself.

---

### Q2.2: What is `epoll` and why do we need it?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:** `epoll` is a Linux kernel facility for monitoring multiple file descriptors for I/O readiness. It solves the "how do I wait on two sockets at once without blocking on either one?" problem.
> - `epoll_create1(0)` → creates an epoll instance (returns a file descriptor).
> - `epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event)` → registers a socket for monitoring.
> - `epoll_wait(epoll_fd, events, max, timeout)` → blocks until at least one registered FD has activity.
> **Depth Upgrade:** Compare: `select()` copies the entire FD set to kernel on every call (O(N)). `epoll` uses a persistent kernel data structure — you register once, query many times (O(1) per event). This is why M1 uses `select()` (only 1 FD) but M2 uses `epoll` (need efficiency with 2+ FDs and potentially many events).

---

### Q2.3: What does "non-blocking" mean for our sockets, and why does M2 need it?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:** By default, `recvfrom()` on a UDP socket **blocks** — it freezes the thread until a packet arrives. In M2, if we block waiting for a client query, we can't simultaneously receive upstream responses (or vice versa).
> - `fcntl(fd, F_SETFL, O_NONBLOCK)` makes the socket non-blocking.
> - Now `recvfrom()` returns immediately with `errno = EAGAIN` if no data is available.
> - The inner `while(1)` drain loop reads packets until it gets `EAGAIN`, then breaks back to `epoll_wait()`.
> **Depth Upgrade:** Explain the alternative: multi-threading (one thread per socket). But threads add complexity (locking, race conditions on the shared linked list). Single-threaded event loops avoid these problems entirely — this is the model used by Redis, nginx, and Node.js.

---

### Q2.4: What is byte order (endianness) and why does our code call `ntohs()`?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:** Network protocols use **big-endian** (most significant byte first). Most x86 CPUs use **little-endian** (least significant byte first).
> - `ntohs()` = Network TO Host Short (16-bit). Converts a 16-bit value from big-endian to whatever your CPU uses.
> - `htons()` = Host TO Network Short. The reverse.
> - Without this conversion, reading `0xA1B2` from a DNS packet on x86 would give you `0xB2A1` — a completely wrong transaction ID.
> **Depth Upgrade:** This is why `uint16_t tx_id = ntohs(*(uint16_t*)buffer)` works: it reads 2 raw bytes from the buffer, casts to a 16-bit integer, then flips the byte order to match our CPU.

---

### Q2.5: What is `recvfrom()` and why not just `recv()`?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:** `recv()` reads data from a **connected** socket (TCP). `recvfrom()` reads data from an **unconnected** socket (UDP) AND tells you WHO sent it by filling in a `sockaddr_in` struct.
> - `recvfrom(fd, buffer, size, flags, (struct sockaddr*)&sender_addr, &addr_len)`
> - Without `recvfrom`, we'd get the data but not know which client sent it.
> - `sendto()` is the reverse: sends data TO a specific address without a persistent connection.
> **Depth Upgrade:** In TCP, `accept()` gives you a new FD per client, so `recv()` on that FD implicitly knows who the client is. In UDP, there's ONE shared FD, so every `recvfrom()` must explicitly capture the sender's address.

---

### Q2.6: What is `struct sockaddr_in` and what information does it hold?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:**
> ```c
> struct sockaddr_in {
>     sa_family_t    sin_family;  // Address family (AF_INET for IPv4)
>     in_port_t      sin_port;    // Port number (network byte order!)
>     struct in_addr sin_addr;    // IPv4 address (network byte order!)
>     char           sin_zero[8]; // Padding (unused, must be zeroed)
> };
> ```
> This is the "return address" on a UDP datagram. When we call `recvfrom()`, the kernel fills this struct with the sender's IP and port. We save it in `request_node.client_addr` so we can `sendto()` the reply back to the exact same IP and port later.
> **Depth Upgrade:** The port in `sin_port` is an **ephemeral port** — the OS randomly assigned it to the client when the client created its socket. It's not port 5353 (that's OUR listening port). The client's port might be something like 49152 or 52100.

---

## Level 3: Data Flow & State Management

> *Can you trace a packet through the code step by step?*

### Q3.1: Walk through the exact lifecycle of a single DNS query through the proxy.
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:** See [L1_Q1_Packet_Lifecycle_and_UDP_Mapping.md](L1_Q1_Packet_Lifecycle_and_UDP_Mapping.md) for the complete walkthrough with diagrams. The key checkpoints are:
> 1. `epoll_wait` → event on `local_socket`
> 2. `recvfrom(local_socket)` → capture `client_addr` + buffer
> 3. Extract `tx_id` from bytes 0–1
> 4. `add(id, client_addr, buffer, size)` → malloc + prepend to linked list
> 5. `sendto(upstream_sock, buffer, ...)` → forward to 8.8.8.8:53
> 6. (later) `epoll_wait` → event on `upstream_sock`
> 7. `recvfrom(upstream_sock)` → read Google's reply
> 8. Validate sender is 8.8.8.8:53
> 9. Extract reply `tx_id`, `find(id)` → get matching node
> 10. `sendto(local_socket, response, ..., node->client_addr)` → route to client
> 11. `delete(id)` → free node + buffer

---

### Q3.2: What is the `request_node` struct and what problem does it solve?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:** `request_node` is an **application-layer state mapping** that compensates for UDP's lack of kernel connection state. It stores:
> - `ID` (16-bit transaction ID) — the lookup key
> - `client_addr` (sockaddr_in) — where to send the reply
> - `addr_len` — size of the address struct
> - `request_Buffer` — heap copy of the original query bytes
> - `request_size` — how many bytes in the query
> - `next` — singly-linked list pointer
> **Depth Upgrade:** Ask yourself: *"Why does the node store a COPY of the query buffer?"* Answer: Because the stack buffer in the event loop gets overwritten on the next `recvfrom()`. If we only saved a pointer, it would be a dangling pointer by the time we need it.

---

### Q3.3: Why does `add()` malloc TWICE — once for the node and once for the buffer?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:** Because the buffer and the node have different lifetimes and sizes.
> - `malloc(sizeof(struct request_node))` — fixed size, holds metadata.
> - `malloc(request_size)` + `memcpy(buffer)` — variable size, holds a copy of the raw DNS query.
> If we embedded a fixed-size buffer in the struct (like `char buffer[1024]`), every node would waste memory for small queries. If we used a pointer to the caller's stack buffer, it would dangle after the function returns.
> **Depth Upgrade:** This is a common C pattern: **struct with a separately-allocated payload**. The tradeoff is that `delete()` must call `free()` twice (once for `request_Buffer`, once for the node) or you leak memory.

---

### Q3.4: How does `find()` work and what is its time complexity?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:** `find(uint16_t id)` is a simple linear search:
> ```c
> request_node *curr = Head;
> while (curr != NULL) {
>     if (curr->ID == id) return curr;
>     curr = curr->next;
> }
> return NULL;
> ```
> - **Time Complexity:** O(N) where N = number of pending requests.
> - **Best Case:** O(1) if the target is at the head.
> - **Worst Case:** O(N) if it's at the tail or doesn't exist.
> **Depth Upgrade:** In a FAANG interview, immediately propose the upgrade: *"For O(1) average lookup, I'd use a hash table. Since transaction IDs are 16-bit (0–65535), a simple direct-indexed array of 65536 slots would work — `request_node *table[65536]` — giving O(1) lookup with zero hash collisions."*

---

## Level 4: Edge Cases & Failure Modes

> *What happens when things go WRONG?*

### Q4.1: What happens if two clients send queries with the same transaction ID?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:** (See REVIEW-SEEDS.md Seed #1)
> 1. Both nodes get added to the list (same `ID`, different `client_addr`).
> 2. `find(id)` returns the FIRST match (most recently added, at list head).
> 3. The response goes to the WRONG client.
> 4. `delete(id)` removes only one node — the other LEAKS forever.
> **Depth Upgrade:** Calculate the probability: with 256 concurrent queries, the birthday problem gives ~50% collision chance. This is not theoretical — it WILL happen under moderate load.

---

### Q4.2: What happens if Google never responds to a query?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:** (See REVIEW-SEEDS.md Seed #8)
> - The `request_node` stays in the linked list forever.
> - There is NO timeout mechanism, NO cleanup sweep, NO eviction.
> - Under sustained packet loss (or a DDoS), the list grows without bound.
> - Eventually: Out-Of-Memory (OOM) crash.
> **Depth Upgrade:** Design the fix: *"I'd add a `timestamp` field to `request_node` (it's already in my design). Then either: (a) use `epoll_wait`'s timeout parameter (currently -1, meaning infinite) to periodically scan and free stale nodes, or (b) check timestamps lazily during every `find()` call and free expired nodes encountered during traversal."*

---

### Q4.3: What happens if `sendto()` to upstream fails?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:** (See REVIEW-SEEDS.md Seed #5)
> - The return value of `sendto()` is **ignored** in the current code.
> - But `add()` was already called — the node is in the list.
> - Since the query never reached Google, no response will come back.
> - The node leaks forever (same as Q4.2).
> **Depth Upgrade:** The fix is to check `sendto()`'s return value and call `delete()` on failure to remove the node we just added.

---

### Q4.4: What happens if `malloc` fails inside `add()`?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:**
> - `add()` returns -1 on `malloc` failure.
> - The event loop **ignores this return value** (line 216 in proxy.c).
> - The query is STILL forwarded to Google via `sendto()`.
> - When Google replies, `find()` returns NULL (no matching node).
> - The response is silently dropped. The client never gets an answer.
> **Depth Upgrade:** There are two failure modes: `malloc` fails for the node itself, or `malloc` fails for the buffer copy. In either case, `add()` cleans up partial allocations before returning -1. The real bug is that the caller doesn't check the return value.

---

### Q4.5: Is 1024 bytes enough for a DNS response buffer?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:** (See REVIEW-SEEDS.md Seed #4)
> - **RFC 1035 (1987):** UDP DNS messages ≤ 512 bytes.
> - **RFC 6891 (EDNS0, 2013):** Modern DNS allows up to 4096+ bytes over UDP.
> - **Reality:** DNSSEC-signed responses regularly exceed 1024 bytes.
> - `recvfrom()` silently truncates to 1024 bytes — no error, no warning.
> - The proxy forwards a **corrupted partial response** to the client.
> **Depth Upgrade:** The fix is straightforward: increase the buffer to 4096 or 65535 bytes. But the deeper question is: *"What if the response is STILL too large?"* That's where TCP fallback (M3) comes in — if the DNS response has the TC (truncation) flag set, you must retry over TCP.

---

## Level 5: System Design & FAANG Deep-Dives

> *Can you reason about this system at production scale?*

### Q5.1: If this proxy handled 10,000 queries per second, where would it break first?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:** The linked list. At 10K QPS with ~50ms average response time, there would be ~500 concurrent pending requests. Every response triggers O(500) linked list walk. That's 5 million node comparisons per second just for lookups. CPU-bound, poor cache locality.
> **Depth Upgrade:** Propose the fix stack: (1) Replace linked list with hash table or direct-indexed array. (2) Add timeout eviction. (3) Consider `io_uring` instead of `epoll` for zero-copy I/O. (4) Use `SO_REUSEPORT` with multiple threads for horizontal scaling.

---

### Q5.2: How would you add caching to this proxy (M3 design)?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:** Before forwarding to upstream, check a local cache keyed by (domain, query type). If hit and TTL hasn't expired, rewrite the transaction ID in the cached response to match the client's ID and send it directly back. If miss or expired, forward to upstream, cache the response, and set a timer based on the response's TTL field.
> **Depth Upgrade:** The tricky parts: (1) You must parse the DNS response to extract TTL values — now `dns_wire.c` becomes relevant to M2. (2) Cache eviction policy (LRU? TTL-only?). (3) Negative caching (should you cache NXDOMAIN responses?). (4) Cache poisoning prevention.

---

### Q5.3: What is the DNS wire format and how is a query structured at the byte level?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:**
> ```
> Bytes 0-1:   Transaction ID (16-bit, random)
> Bytes 2-3:   Flags (QR=0 for query, RD=1 for recursion desired → 0x0100)
> Bytes 4-5:   QDCOUNT (number of questions, usually 1)
> Bytes 6-7:   ANCOUNT (answer count, 0 in queries)
> Bytes 8-9:   NSCOUNT (authority count, 0 in queries)
> Bytes 10-11: ARCOUNT (additional count, 0 in queries)
> Byte 12+:    Question section:
>              - Name: length-prefixed labels (e.g., \x07example\x03com\x00)
>              - QTYPE: 2 bytes (0x0001 = A record)
>              - QCLASS: 2 bytes (0x0001 = IN / Internet)
> ```
> **Depth Upgrade:** Explain DNS name compression: instead of repeating a full name, a response can use a 2-byte pointer (`0xC0 XX`) that says "the name is at offset XX in this packet." Our `pars_reply()` handles compression pointers in the question section but not in answer names — that's a known limitation.

---

### Q5.4: Why does the proxy validate that upstream responses come from 8.8.8.8:53?
**Your Answer:** `___`
**Self-Rating:** `[ ] Solid  [ ] Shaky  [ ] Gap`

> **Answer Tip:** Without sender validation, an attacker on the same network could send spoofed UDP packets to our `upstream_sock` with a fake DNS response containing a malicious IP address. If the attacker guesses the transaction ID (only 16 bits!), the proxy would forward the poisoned response to the client.
> This is called a **DNS cache poisoning attack** (Kaminsky attack, 2008).
> **Depth Upgrade:** Our validation checks both IP and port. But with only 65,536 possible transaction IDs, an attacker can spray spoofed packets and hit a valid ID within seconds. This is why modern DNS uses: (a) source port randomization (EDNS0), (b) DNS cookies, (c) DNSSEC signatures, (d) DNS-over-HTTPS/TLS.

---

## Summary Scorecard

Fill this out after answering all questions:

| Level | Questions | Solid | Shaky | Gap |
|---|---|---|---|---|
| L0: Project Identity | 4 | __ | __ | __ |
| L1: Architecture | 4 | __ | __ | __ |
| L2: Core Concepts | 6 | __ | __ | __ |
| L3: Data Flow | 4 | __ | __ | __ |
| L4: Edge Cases | 5 | __ | __ | __ |
| L5: System Design | 4 | __ | __ | __ |
| **Total** | **27** | **__** | **__** | **__** |

**Your Known Knowns Score:** `__/27 Solid`
**Known Unknowns Identified:** `__/27 Shaky + Gap`

---

## What Happens Next

- **Solid answers** → These are your Known Knowns. You own this material.
- **Shaky answers** → These are Known Unknowns. We target them in Phase 2 (During).
- **Gap answers** → These might be Unknown Unknowns. We do a Blind Spot Pass to find more.

After you complete this scorecard, we will:
1. Build a visual **Knowledge Map Canvas** showing your mastery topology.
2. Run a **Blind Spot Pass** on your Gap areas to find deeper Unknown Unknowns.
3. Design targeted **Phase 2 teaching sessions** for each Shaky/Gap cluster.
