# 🧜‍♂️ Mermaid Visualizer: DNS Proxy Architecture & Flow

> **Built with Axton Liu's `mermaid-visualizer` skill specification.**  
> Optimized for Obsidian, GitHub, and FAANG technical presentations.

---

## 1️⃣ End-to-End Architecture (Swimlane Pattern)

```mermaid
graph TB
    subgraph client["① Client Zone"]
        C1["dig @127.0.0.1 -p 5353"]
        C2["Client Socket Address<br/>IP + Ephemeral Port"]
    end

    subgraph proxy["② Asynchronous Proxy Core"]
        E1["epoll_wait() Loop"]
        PFD["proxy_fd (port 5353)"]
        SFD["dns_server_fd (random port)"]
    end

    subgraph memory["③ Application State Machine"]
        M1["request_node Linked List"]
        M2["State Mapping:<br/>tx_id ➡️ client_addr"]
    end

    subgraph upstream["④ Upstream Resolver"]
        G1["Google Public DNS<br/>8.8.8.8:53"]
    end

    C1 -->|UDP Query| PFD
    PFD -->|EPOLLIN| E1
    E1 -->|① Extract tx_id & client_addr| M1
    M1 -.->|② Save Mapping| M2
    E1 -->|③ sendto| SFD
    SFD ==>|Forward Query| G1

    G1 ==>|UDP Reply| SFD
    SFD -->|EPOLLIN| E1
    E1 -->|④ Lookup tx_id| M1
    M1 -.->|⑤ Match Found| M2
    M2 -->|⑥ Return client_addr| E1
    E1 -->|⑦ sendto| PFD
    PFD -->|DNS Answer| C1

    style C1 fill:#a5d8ff,stroke:#1e40af,stroke-width:2px
    style G1 fill:#b2f2bb,stroke:#15803d,stroke-width:2px
    style M1 fill:#fff3bf,stroke:#f59e0b,stroke-width:2px
    style M2 fill:#fff3bf,stroke:#f59e0b,stroke-width:2px
    style E1 fill:#d0bfff,stroke:#5f3dc4,stroke-width:2px
```

---

## 2️⃣ Socket Multiplexing & Memory Leak Trap (Comparison Flow)

```mermaid
graph LR
    subgraph normal["🟢 Normal Happy Path"]
        N1["Query Arrives"] --> N2["malloc request_node"]
        N2 --> N3["Send to 8.8.8.8"]
        N3 --> N4["Google Replies"]
        N4 --> N5["Match & Route"]
        N5 --> N6["free(node) ✅"]
    end

    subgraph leak["🔴 The OOM Leak Trap (Unanswered Query)"]
        L1["Query Arrives"] --> L2["malloc request_node"]
        L2 --> L3["Send to 8.8.8.8"]
        L3 -.->|Packet Dropped / Timeout| L4["⚠️ No Reply Ever Arrives"]
        L4 --> L5["Node Never Unlinked!"]
        L5 ==>|10,000+ Leaked Nodes| L6["💥 Out Of Memory (OOM) Crash!"]
    end

    style N6 fill:#b2f2bb,stroke:#15803d,stroke-width:2px
    style L6 fill:#ffc9c9,stroke:#c92a2a,stroke-width:3px
    style L5 fill:#ffd8a8,stroke:#d9480f,stroke-width:2px
```
