# REVIEW SEEDS — dns-proxy-cache

> Areas in THIS implementation worth questioning.
> Each seed is a real concern found by reading the code — not a generic checklist item.
> Priority: 🔴 likely bug, 🟡 design concern, 🔵 worth understanding deeper

---

## 🔴 1. Transaction ID Collision

**File:** `proxy.c` → `add()`, `find()`, `delete()`
**Line:** ~30, ~54, ~63

**The question:** What happens when two different clients send queries with the **same** 16-bit transaction ID?

**What the code does:** `add()` blindly prepends to the linked list. `find()` returns the **first** match. So if Client A and Client B both use ID `0x1234`:
1. Both get added to the list
2. When the upstream response arrives, `find(0x1234)` returns whichever was added last (head)
3. The response goes to the wrong client
4. `delete(0x1234)` removes only the first match — the other node leaks forever

**How likely:** Transaction IDs are 16-bit (65,536 values). Under light load, collisions are rare. Under moderate load with many concurrent clients, birthday-problem math kicks in — at ~256 concurrent queries, collision probability hits ~50%.

**Fix ideas:**
- Key by `(ID, client_addr, client_port)` tuple instead of ID alone
- Rewrite the ID to a proxy-assigned unique value before forwarding upstream (ID remapping)

---

## 🟡 2. Response Validation Completeness in is_dns_reply_valid

**File:** `dns_wire.c` → `is_dns_reply_valid()` (line 71–85)

**What it checks:**
- Buffer ≥ 12 bytes ✓
- QR bit set ✓
- Transaction ID match ✓
- RCODE == 0 ✓
- ANCOUNT > 0 ✓

**What it doesn't check:**
- Opcode (byte 2, bits 1–4) — could be an inverse query or status response
- TC (truncation) flag — if set, the response is incomplete and needs TCP retry
- Whether the question section matches what was asked
- Whether QDCOUNT == 1 (matching the original query)

**Risk:** Low for M1 (talking to trusted 8.8.8.8). Higher if the proxy ever serves untrusted networks.

---

## 🟡 3. Wire Parsing Assumptions — Fixed Offsets and No Compression Support

**File:** `dns_wire.c` → `pars_reply()` (line 87–133)

**Fragile assumptions:**
- Assumes the question section uses **uncompressed** labels (rejects `0xC0` pointers at line 103)
- Skips exactly 5 bytes after the question name (null + QTYPE + QCLASS) — correct but unnamed/undocumented
- Assumes the **first** answer record is at a fixed offset after the question
- Hardcodes `offset += 10` to skip answer NAME + TYPE + CLASS + TTL — this only works if the answer name is a 2-byte compression pointer (common but not guaranteed)
- Requires RDLENGTH == 4 — only handles A records, silently fails on AAAA, CNAME, etc.

**Real-world breakage:** A CNAME answer before the A record will cause incorrect offset calculation and a silent `return NULL`.

---

## 🔴 4. Buffer Sizing — 1024 Bytes

**File:** `proxy.c` lines 196, 223 | `main.c` line 107

**The question:** Is 1024 bytes enough for DNS responses?

**RFC 1035 says:** UDP DNS messages should be ≤ 512 bytes.
**Reality (EDNS0, RFC 6891):** Modern DNS commonly uses payloads up to 4096 bytes. DNSSEC-signed responses regularly exceed 1024 bytes.

**What happens:** `recvfrom()` silently truncates the packet. The proxy forwards a corrupted partial response to the client. No error is raised.

**Impact:** The proxy will silently break for:
- DNSSEC-enabled domains
- Domains with many A records or large TXT records
- Any EDNS0 response > 1024 bytes

---

## 🟡 5. Error-Path Behavior — sendto/recvfrom Failures

**File:** `proxy.c` event loop (lines 195–259)

**What happens when `sendto()` to upstream fails (line 218)?**
- The return value is **ignored**. The node was already added to the linked list via `add()`.
- The node sits in the list forever with no timeout — memory leak + stale state.

**What happens when `sendto()` back to the client fails (line 256)?**
- The return value is **ignored**. The node is deleted anyway via `delete(id)`.
- The client gets no response and no retry.

**What happens when `add()` fails (returns -1)?**
- The return value is **ignored** (line 216). The query is still forwarded upstream, but there's no tracking node. When the response comes back, `find()` returns NULL and the response is dropped.

---

## 🟡 6. Linked List Performance Under Load

**File:** `proxy.c` → `find()`, `delete()`

**Current complexity:**
- `find()`: O(N) linear scan
- `delete()`: O(N) linear scan
- `add()`: O(1) prepend

**At scale:** With 1000 concurrent queries, every upstream response triggers an O(1000) linked list walk. For a DNS proxy handling heavy traffic, this becomes a bottleneck.

**Better structures:** Hash table keyed by transaction ID (O(1) average lookup). Even a simple 65536-entry array indexed by ID would work since IDs are 16-bit.

---

## 🔵 7. epoll Edge Cases

**File:** `proxy.c` lines 140–179, 182–265

**Level-triggered vs edge-triggered:** The code uses `EPOLLIN` (level-triggered, the default). This is the **safe** choice — it won't miss events. But the inner `while(1)` drain loops (lines 195, 222) are written as if using edge-triggered mode. With level-triggered, a single `recvfrom()` per `epoll_wait()` wakeup would also work (though draining is more efficient).

**EINTR handling:** The `epoll_wait` loop handles `EINTR` (line 186) ✓. Good.

**Events array size:** `events[10]` — only 10 slots. Since we only have 2 fds registered, this is fine. But it hardcodes a limit if more sockets are added later.

**Missing:** No `EPOLLERR` or `EPOLLHUP` handling. If a socket errors out, the event loop may behave unexpectedly.

---

## 🔴 8. Memory Leak Paths in request_node List

**File:** `proxy.c` — the entire linked list lifecycle

**Leak scenarios:**
1. **sendto to upstream fails silently** → node stays in list, no cleanup path
2. **Upstream never responds** (packet loss, server down) → node stays forever, no timeout/eviction
3. **ID collision** → duplicate nodes, only one gets deleted per response
4. **Program termination** (`break` from event loop) → all remaining nodes leak (lines 267–271 close sockets but never free the list)

**What's missing:** A timeout mechanism. Real DNS proxies expire pending requests after 5–10 seconds. Without this, a burst of unanswered queries will grow the list without bound.

---

## 🟡 9. Hardcoded Transaction ID in build_dns_query

**File:** `dns_wire.c` → `build_dns_query()` line 43

**The problem:** Transaction ID is hardcoded to `0xAAAA`. Every query built by `build_dns_query()` uses the same ID.

**Impact on M1:** Not a real problem — M1 sends one query and exits.

**Impact if reused:** If this function were used to build queries in the proxy (M2), every forwarded query would collide. Currently M2 forwards raw client bytes and doesn't call `build_dns_query()`, so this is latent.

**RFC 1035 §4.1.1:** The ID should be random to prevent cache poisoning attacks.

---

## 🟡 10. proxy.h — Stale/Misleading Header

**File:** `proxy.h`

**Contents:** Declares `int socket(int domain, int type, int protocol);` — this is just a forward declaration of the POSIX `socket()` function, not a proxy-specific interface.

**Risk:** If anyone includes this header, it may conflict with system headers. It appears to be leftover scaffolding from early development and isn't included by any current source file.

---

## 🔵 11. Windows Path Is a Dead End

**File:** `proxy.c` lines 261–264

**What it does:** On Windows, the event loop body is just `Sleep(1000)` — no I/O handling whatsoever.

**Why:** `epoll` doesn't exist on Windows. The proxy is Linux-only at runtime. The Windows build compiles but doesn't actually proxy anything.

**Question to explore:** Would `WSAPoll()` or `IOCP` be the right Windows equivalent? Or is WSL the intended deployment target?

---

## 🔵 12. Upstream Sender Validation

**File:** `proxy.c` lines 237–241

**What it does:** Checks that the upstream response came from `8.8.8.8:53` (IP and port). Good security practice for UDP.

**Subtle issue:** Uses `perror("Dropped packet from untrusted source")` — but this isn't an `errno`-based error. `perror` will print a misleading errno message. Should use `fprintf(stderr, ...)` instead.

---

## 🟡 13. No Signal Handling / Graceful Shutdown

**File:** `proxy.c`

**The loop:** `while(1)` runs forever. There's no signal handler for `SIGTERM`/`SIGINT`. Socket cleanup code after the loop (lines 267–271) is unreachable.

**Impact:** On `Ctrl+C`, the OS reclaims sockets and memory, so nothing truly leaks. But the linked list nodes are never freed, and if there were file-based logging or cache persistence (M3), data could be lost.

---

## 🔵 14. encode_dns_name Output Buffer Not Bounds-Checked

**File:** `dns_wire.c` → `encode_dns_name()` line 6–35

**The risk:** The output buffer `out` has no size parameter. The caller (`build_dns_query`) allocates 512 bytes total, and the name encoding starts at offset 12. A hostname longer than ~490 characters could overflow the buffer. Realistic hostnames are ≤ 253 characters, so this is unlikely but not guarded.
