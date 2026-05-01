# DNS Proxy Relay in C

`dns-proxy-cache` has been cleaned up and now documented as a **DNS proxy relay** implementation (cache layer is planned, not implemented yet).  
It includes:

- a DNS query client (`dns_query`) that builds wire-format packets and parses A-record replies,
- a UDP DNS relay proxy (`dns_proxy`) that listens locally and forwards to an upstream resolver,
- cross-platform socket setup for Windows and POSIX builds.

The codebase is intentionally small and explicit, so packet flow and network behavior are easy to inspect.

## What It Implements

### 1. DNS wire encoding/decoding

`src/dns_wire.c` implements:

- `encode_dns_name` for label-based DNS name serialization (`example.com` -> `07 example 03 com 00`),
- `build_dns_query` for constructing a minimal DNS A-query packet,
- `parse_dns_reply` for extracting the first IPv4 A-answer from a response payload.

### 2. Query client

`src/dns_query_client.c`:

- creates a UDP socket,
- sends a generated DNS query to a resolver (default `8.8.8.8:53`),
- waits with `select()` (3-second timeout),
- parses and prints the resolved IPv4 address.

### 3. UDP relay proxy

`src/dns_proxy_server.c`:

- binds a local UDP socket (default `127.0.0.1:5353`),
- forwards inbound DNS packets to upstream DNS (default `8.8.8.8:53`),
- tracks in-flight requests by DNS transaction ID,
- routes upstream replies back to the original client,
- evicts stale pending entries after timeout.

## Current Architecture

```text
apps/
  dns_query_main.c
  dns_proxy_main.c

include/
  dns_wire.h
  dns_query_client.h
  dns_proxy_server.h
  net_platform.h

src/
  dns_wire.c
  dns_query_client.c
  dns_proxy_server.c
  net_platform.c
```

Separation of concerns:

- `src/dns_wire.c` = packet serialization/parsing only
- `src/dns_query_client.c` = one-shot query flow
- `src/dns_proxy_server.c` = relay event loop and pending-request lifecycle
- `src/net_platform.c` = socket stack init/cleanup abstraction

## Request Flow

### Query binary (`dns_query`)

1. Build DNS query bytes.
2. Send packet to configured resolver.
3. Wait for response with timeout.
4. Parse first A-record and print IPv4 text.

### Proxy binary (`dns_proxy`)

1. Receive client DNS query on local socket.
2. Read DNS transaction ID.
3. Store `(id, client_addr, timestamp)` in pending list.
4. Forward packet upstream.
5. Receive upstream reply.
6. Match reply ID to pending entry.
7. Forward reply to original client and remove pending entry.

## Build and Run

### CMake (preferred)

```bash
cmake -S . -B build
cmake --build build
```

Produced targets:

- `dns_query`
- `dns_proxy`

### GCC (Windows/MSYS2 example)

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Iinclude src/dns_wire.c src/net_platform.c src/dns_query_client.c apps/dns_query_main.c -lws2_32 -o dns_query.exe
gcc -std=c11 -Wall -Wextra -Wpedantic -Iinclude src/dns_wire.c src/net_platform.c src/dns_proxy_server.c apps/dns_proxy_main.c -lws2_32 -o dns_proxy.exe
```

### Run examples

```bash
./dns_query
./dns_proxy
```

On Windows:

```bash
dns_query.exe
dns_proxy.exe
```

## Example Usage

To test proxy mode with `dig`:

```bash
dig @127.0.0.1 -p 5353 example.com
```

Expected behavior: request is forwarded upstream, reply is relayed back, and pending request is removed.

## Limitations (Current Scope)

- No TTL-based response cache yet.
- No TCP fallback path (UDP only).
- Matching uses only DNS transaction ID (sufficient for local learning scenarios, not hardened multi-tenant production behavior).
- Parser extracts first A record only.
- No metrics/exported observability interface.

## Future Work

1. Add cache module with TTL-aware insert/lookup/eviction.
2. Add TCP handling for truncation/large responses.
3. Add safer matching tuple (ID + question fingerprint + client endpoint).
4. Add structured logs and query statistics.
5. Expand parser support for AAAA/CNAME and richer answer handling.

## Learning Outcomes

This project demonstrates practical systems-level skills:

- DNS wire protocol manipulation,
- cross-platform socket programming,
- event-loop driven UDP relay design,
- manual memory ownership in C,
- incremental refactoring into maintainable modules.
