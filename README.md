# DNS Proxy Relay in C

This repository contains a compact DNS networking project written in C.  
It currently implements two binaries:

- **`dns_query`**: builds a DNS A-record query, sends it over UDP, and parses the IPv4 answer.
- **`dns_proxy`**: runs a local UDP relay (`127.0.0.1:5353`) and forwards requests to an upstream resolver (`8.8.8.8:53`).

> Note: the repository name is still `dns-proxy-cache`, but the current implementation is a relay. A true cache layer is planned and documented in the roadmap.

## Project Objectives

The project is designed to demonstrate:

- DNS wire-format handling without external libraries,
- event-driven UDP socket programming,
- disciplined memory ownership in C,
- clean modularization for future protocol and cache extensions.

## Architecture

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

### Module responsibilities

- **`src/dns_wire.c`**: DNS name encoding, query packet construction, and reply parsing.
- **`src/dns_query_client.c`**: end-to-end client flow (build -> send -> wait -> parse).
- **`src/dns_proxy_server.c`**: UDP relay loop, pending-query tracking, and timeout eviction.
- **`src/net_platform.c`**: Winsock/POSIX startup and cleanup abstraction.

## How It Works

### `dns_query`

1. Encodes the hostname into DNS label format.
2. Builds a DNS query packet (A, IN).
3. Sends the packet to the configured resolver.
4. Waits for a reply with `select()`.
5. Extracts and prints the first IPv4 A-answer.

### `dns_proxy`

1. Receives DNS packets from local clients.
2. Reads the DNS transaction ID.
3. Stores pending metadata `(id, client_addr, timestamp)`.
4. Forwards packet to upstream resolver.
5. Receives upstream reply and matches by ID.
6. Sends reply back to the original client.
7. Evicts stale pending entries on timeout.

## Build

### GCC (Windows/MSYS2)

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Iinclude src/dns_wire.c src/net_platform.c src/dns_query_client.c apps/dns_query_main.c -lws2_32 -o dns_query.exe
gcc -std=c11 -Wall -Wextra -Wpedantic -Iinclude src/dns_wire.c src/net_platform.c src/dns_proxy_server.c apps/dns_proxy_main.c -lws2_32 -o dns_proxy.exe
```

### GCC / Clang (Linux/macOS)

```bash
cc -std=c11 -Wall -Wextra -Wpedantic -Iinclude src/dns_wire.c src/net_platform.c src/dns_query_client.c apps/dns_query_main.c -o dns_query
cc -std=c11 -Wall -Wextra -Wpedantic -Iinclude src/dns_wire.c src/net_platform.c src/dns_proxy_server.c apps/dns_proxy_main.c -o dns_proxy
```

## Run

```bash
./dns_query
./dns_proxy
```

Windows:

```bash
dns_query.exe
dns_proxy.exe
```

### Proxy test example

```bash
dig @127.0.0.1 -p 5353 example.com
```

## Current Scope and Limitations

- UDP only (no TCP fallback for truncated responses).
- No TTL cache implemented yet.
- Reply matching is transaction-ID based.
- Parser currently focuses on extracting the first IPv4 A-record.
- No metrics endpoint or structured telemetry.

## Roadmap

1. Add TTL-aware cache storage and lookup.
2. Add TCP handling for large/truncated DNS replies.
3. Strengthen request/reply correlation beyond transaction ID.
4. Expand parser support (AAAA, CNAME, additional records).
5. Add structured logging and runtime stats.

## Why This Project Matters

This codebase is a practical systems-programming exercise in protocol-level networking. It favors explicit control flow, small focused modules, and implementation clarity over framework abstraction.
