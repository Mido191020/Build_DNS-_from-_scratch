# dns-proxy-cache

[![Language](https://img.shields.io/badge/language-C11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Build](https://img.shields.io/badge/build-CMake-green.svg)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows-lightgrey.svg)](#)

A high-performance, lightweight DNS resolver client and concurrent, non-blocking UDP proxy server built from scratch in C11 using POSIX sockets and `epoll`.

---

## Why this project?
When developing network infrastructure, treating protocols as simple function calls is a common pitfall. DNS is a raw **packet-oriented message protocol** where every request and response is a structured byte stream containing headers, queries, and variable-length resource records. This project demonstrates how to build cross-platform DNS wire format encoders, parse complex network responses, and multiplex concurrent traffic using low-level OS primitives.

---

## Features
- **Raw Wire Engine (`dns_wire.c`):** Manual bitwise serialization and deserialization of DNS standard queries and responses (RFC 1035).
- **A-Record Resolution:** Encodes dotted-decimal hostnames (e.g., `example.com`) to length-prefixed DNS labels and extracts IPv4 addresses from A-record answer payloads.
- **Concurrent epoll Proxy (`proxy.c`):** An asynchronous UDP proxy listening on port `5353` that handles concurrent client requests using Linux `epoll` edge/level-triggered multiplexing.
- **State Tracking & Routing:** Singly linked request tracker mapped by 16-bit Transaction IDs to correctly route async upstream responses back to originating clients.
- **Origin Verification:** Validation of source IP and port for all packets received from upstream resolvers to guard against packet spoofing.
- **Cross-Platform Compatibility:** Single-shot query resolution works natively on Linux and Windows (using WSAStartup initialization helpers).

---

## Quickstart (Get Running in 2 Minutes)

### 1. Build the Code
#### Option A: Command Line
Ensure you have CMake and a C compiler installed, then run:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

#### Option B: CLion / IDE
Simply open this directory as a project in CLion. The IDE will automatically configure CMake using the `CMakeLists.txt` file and allow you to build and run `dns_query` and `dns_proxy` targets directly.


### 2. Start the DNS Proxy (Linux / WSL)
Start the proxy to listen locally on port `5353` and forward queries to Google DNS (`8.8.8.8`):
```bash
./build/dns_proxy
```

### 3. Send a DNS Query
In another terminal, send a query through the proxy using `dig`:
```bash
dig @127.0.0.1 -p 5353 google.com A
```
Alternatively, run the included Python test client:
```bash
python dns_test.py
```

---

## Configuration
All configuration parameters are currently hardcoded in source files for speed and simplicity. 

- **Proxy Bind Port:** `127.0.0.1:5353` (defined in `proxy.c`)
- **Upstream DNS Resolver:** `8.8.8.8:53` (defined in `proxy.c`)
- **Default Resolution Target:** `example.com` (defined in `main.c` for `dns_query`)

For instructions on how to modify these values or compile with specific warning levels, see [docs/TECHNICAL.md](file:///D:/clion/dns-proxy-cache/docs/TECHNICAL.md#L45-L65).

---

## Usage Examples

### Executing the Resolver (`dns_query`)
To query `example.com` directly using the single-shot client:
```bash
$ ./build/dns_query
Resolved IP: 93.184.215.14
```

### Testing the Proxy with `dns_test.py`
```bash
$ python dns_test.py
Sending DNS query to ('127.0.0.1', 5353)...
Received response from ('127.0.0.1', 5353): 12348180000100010000000006676f6f676c6503636f6d0000010001c00c00010001000000780004ad251a2e
Response Transaction ID: 1234
Resolved IP from response: 173.37.26.46
```

---

## Project Structure

```text
├── .github/
│   └── pull_request_template.md  # Standard template for pull requests
├── docs/
│   ├── TECHNICAL.md              # Full architecture, protocol details & limits
│   ├── CODE_MAP.md               # Function-by-function mapping of source code
│   └── PROGRESS.md               # Tracking sheet for educational milestones
├── CMakeLists.txt                # Build configuration for CMake
├── CONTRIBUTING.md               # Code standards, git conventions, PR workflow
├── dns_wire.c / dns_wire.h      # DNS wire packet generation and parsing
├── main.c                        # M1 single-shot client implementation
├── proxy.c / proxy.h             # M2 epoll UDP proxy implementation
├── dns_test.py                   # Python validation client
└── wsa_init_helper.c             # Winsock initialization constructor for Windows
```

---

## Contributing
We welcome contributions! Please review [CONTRIBUTING.md](file:///D:/clion/dns-proxy-cache/CONTRIBUTING.md) for details regarding our branch workflow, Conventional Commit guidelines, and code standards.

---

## Technical Documentation
For a deep dive into the proxy state machine, protocol limitations, security analysis, and packet lifecycles, read [docs/TECHNICAL.md](file:///D:/clion/dns-proxy-cache/docs/TECHNICAL.md).

---

## License
This project is open-source. License unspecified. Refer to repository owner.
