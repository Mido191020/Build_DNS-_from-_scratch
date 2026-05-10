# build DNS

`build DNS` is a custom DNS proxy cache written in C with POSIX sockets. It listens locally for DNS queries on `127.0.0.1:5300`, forwards cache misses upstream, and returns cached or resolved answers with TTL-aware behavior.

## What this project is about

This project is split into three core concerns:

- **`dns_wire.c`**: DNS wire-format encoding and decoding
- **`dns_cache.c`**: TTL cache management
- **`proxy.c`**: UDP/TCP proxy flow and event-loop logic

The main idea from the playbook is that DNS is a **message protocol**, not a simple `ask -> get IP` function call. Every request and response is a structured packet with a header, question, and answer sections.

## Milestone 1: DNS Wire Engine

The first milestone is a standalone DNS wire module that:

1. Builds an A-record query with a 12-byte DNS header plus question section.
2. Uses the **trampoline pattern**: build on the stack first, then `memcpy()` to a heap buffer returned to the caller.
3. Sends the packet to `8.8.8.8:53` over UDP.
4. Waits with `select()` using a 3-second timeout.
5. Parses the reply, skips header and question, and extracts the first A-record IP with `inet_ntop()`.

### Ownership rule

If a function returns a heap buffer, the caller owns it and must free it on every exit path, including errors and timeouts.

## DNS packet structure

### Header

The DNS header carries the message ID, flags, and counters such as:

- number of questions
- number of answers
- number of authority records
- number of additional records

The ID is critical because it matches a response to the original query.

### Question

The question tells the resolver:

- the domain name
- the record type
- the class

Example:

- `example.com`
- `A`
- `IN`

### Answer

The answer section contains the resolved data. For an A record, that usually means:

- name
- type
- class
- TTL
- IPv4 address data

## Wire-format example

DNS names are encoded as length-prefixed labels:

```text
example.com -> 07 example 03 com 00
```

The IP is not returned as text in the packet. It is carried as raw bytes and then formatted into dotted-decimal form by the client.

## End-to-end flow

1. The client wants the IP for a hostname like `example.com`.
2. The hostname is encoded into DNS wire format.
3. A DNS request packet is built.
4. The packet is sent to an upstream resolver over UDP.
5. The resolver decodes the request and looks up the record.
6. The resolver returns a structured response packet.
7. The client parses the response and extracts the IP.

## Memory safety rules

- Every `malloc()` must have a matching `free()` on every path.
- Never read memory after `free()`.
- Check `realloc()` before overwriting the original pointer.
- Update linked-list pointers before freeing a node.
- Run leak checks before moving to the next milestone.

## Build

### Example build commands

```bash
gcc -o dns-proxy-cache main.c dns_wire.c dns_cache.c proxy.c
./dns-proxy-cache
```

Or with `make`:

```bash
make
./dns-proxy-cache
```

## Debugging tools

- `dig` for query testing
- `valgrind` for leak and invalid access checks
- `tcpdump` for packet inspection

## Planned milestones

1. **Milestone 1** — DNS wire engine
2. **Milestone 2** — UDP relay loop
3. **Milestone 3** — TTL cache
4. **Milestone 4** — TCP fallback with buffering
5. **Milestone 5** — polish, stats, and leak-free cleanup

## Project files

- `dns_wire.c` — DNS packet encoding/decoding
- `dns_cache.c` — cache storage and eviction
- `proxy.c` — networking and request dispatch
- `main.c` — entry point used for testing

## Notes

The playbook’s core message is simple: understand the packet, own the memory, and keep the network flow separated from wire-format logic.
