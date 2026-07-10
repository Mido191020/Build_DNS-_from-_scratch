# Session: 2026-07-06 — Phase 1: Known Knowns & Memory Refresh (Q&A Interview Mode)

## Goal
Validate baseline understanding of the DNS Proxy M1/M2 codebase, refresh memory, identify weak spots, and practice FAANG-level technical communication through progressive Socratic Q&A.

---

## Phase: Pre-Work (Strategy & Rules)
- **Mode:** Socratic Interview & Q&A
- **Progression:** Level 1 (Architecture & Flow) → Level 2 (Low-Level C & epoll Mechanics) → Level 3 (Edge Cases & System Limits)
- **Documentation:** Every question, answer, critique, and identified weakness will be logged here in real-time.

---

## Q&A Log

### Level 1: High-Level Architecture & Data Flow

#### Question 1: Client Query Lifecycle & Stateless UDP Mapping
**Status:** Completed via Visual Architecture & Obsidian Canvas
**Question:** When a UDP packet arrives from a client on port 5353, walk through the exact lifecycle of that packet from the moment `epoll_wait` wakes up until the query is forwarded upstream to `8.8.8.8:53`. How do we distinguish client queries from upstream responses, and what state must we save in memory before forwarding?
**User Answer / Collaborative Exploration:** 
- Explored via deep architectural diagrams and visual workflows.
- Generated comprehensive Markdown Study Guide: [L1_Q1_Packet_Lifecycle_and_UDP_Mapping.md](file:///D:/clion/dns-proxy-cache/docs/L1_Q1_Packet_Lifecycle_and_UDP_Mapping.md)
- Generated interactive Obsidian Canvas diagram: [L1_Q1_Packet_Lifecycle_Canvas.canvas](file:///D:/clion/dns-proxy-cache/docs/L1_Q1_Packet_Lifecycle_Canvas.canvas)
**Mentor Critique & Feedback:**
- **Key Takeaway:** Unlike TCP proxies that rely on kernel-level connection streams, UDP proxies must build an application-layer state machine (`struct request_node` linked list) to map the 16-bit DNS Transaction ID to the originating client's `sockaddr_in`.
- **FAANG Interview Insight:** A singly linked list gives O(1) insertion on query arrival, but O(N) lookup on upstream response. At FAANG scale (10,000+ QPS), this requires migration to a bucketed Hash Table or Red-Black tree.
**Identified Weaknesses / Unknowns to Target Next:**
- How do we handle Transaction ID collisions (when two clients generate the same random 16-bit ID)?
- How do we prevent OOM memory leaks when Google never replies (missing timeouts/eviction)?

---

#### Question 2: Memory Management & The OOM Leak Trap (Level 2)
**Status:** Ready / Pending
**Question:** In our `proxy.c` event loop, what happens to the `request_node` memory if an upstream DNS UDP packet is dropped by the network or Google never responds? Walk me through the exact memory leak path, explain why this will cause a production crash under load, and design a C/epoll mechanism to fix it.

---

## Summary & Next Steps
*(To be updated at the end of the session)*
