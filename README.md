# Peer-to-Peer File Sharing System

## Overview
This project is a peer-to-peer (P2P) distributed file sharing system implemented in C++. It features a dual-tracker architecture for redundancy and high availability. Clients connect to the trackers to get metadata about users, groups, and files, but transfer file data directly between each other. The system is designed to be resilient, allowing clients to continue operating as long as at least one tracker is online.

The current implementation focuses on the robust user and group management backbone, including a fully synchronized, thread-safe state between the two trackers and a resilient client capable of automatic failover.

---

## Compilation and Execution

### Compilation
A `Makefile` is provided for easy compilation. Simply run the `make` command from the root directory of the project.

```bash
# Clean previous builds and compile both tracker and client
make clean && make
```

This will produce two executables: `tracker.out` and `client.out`.

### Running the System

The system requires at least three separate terminals to run: one for each tracker and one for the client.

**1. Start Tracker 1 (Terminal 1)**

```bash
./tracker.out tracker_info.txt 1
```

It will start listening and may show a "Failed to connect to peer" message, which is normal.

**2. Start Tracker 2 (Terminal 2)**

```bash
./tracker.out tracker_info.txt 2
```

It will start and connect to Tracker 1. Immediately after, you will see a "Handshake received" message in Terminal 1 as it connects back. At this point, the trackers are fully synced.

**3. Start the Client (Terminal 3)**

```bash
./client.out 127.0.0.1:9909 tracker_info.txt
```

The client will connect to a tracker, and you can begin issuing commands.

-----

## System Architecture and Design Choices

### Tracker Architecture

The tracker is a single-threaded, event-driven server that uses the `select()` system call for I/O multiplexing. This allows a single process to efficiently manage multiple connections (from clients and its peer tracker) without the overhead of creating a new thread for each connection. All shared state is stored in-memory and protected by a single mutex to ensure thread safety.

### Client Architecture

The client is a command-line application that maintains a persistent connection to one of the trackers. It has built-in failover logic; if its connection to the current tracker is lost, it automatically attempts to connect to the other tracker listed in `tracker_info.txt`, providing a seamless user experience in the event of a tracker failure. The client crash issue on disconnect is handled by ignoring the `SIGPIPE` signal, a standard practice for robust network clients.

### Network Protocol

A custom TCP-based protocol is used for all communication. To ensure the integrity of commands, every message is prefixed with a 4-byte header indicating the length of the message. This message framing prevents issues with partial reads from the TCP stream, guaranteeing that the application always processes whole commands.

-----

## Synchronization Approach

* **Approach:** An **active-active model with a primary-update** strategy was chosen. When a client sends a state-changing command (e.g., `create_user`, `accept_request`), the tracker that receives the request acts as the "primary" for that transaction. It updates its local state first and then forwards a special `SYNC` command to its peer. The peer receives this command and applies the exact same change to its own state.

* **Startup Handshake:** To guarantee a two-way connection is established at startup, a simple handshake is used. When the second tracker connects to the first, it sends a `SYNC_INIT` message. This prompts the first tracker, which may have failed its initial connection attempt, to connect back, ensuring both are ready to send and receive updates.

* **Justification:** For a two-node system, this approach is simple, efficient, and provides strong consistency. It avoids the complexity of distributed consensus protocols like Paxos or Raft, which would be overkill. As long as the two trackers can communicate, their state remains an identical mirror, fulfilling the redundancy requirement. Read-only operations like `list_groups` do not generate sync traffic, which is efficient.

-----

## Handling New Connections

The tracker handles new client connections in a non-blocking manner using `select()`. The main listening socket is monitored for activity. When `select()` indicates an incoming connection, `accept()` is called to create a new socket for the client. This new file descriptor is then added to the set of sockets monitored by `select()`, allowing the server to handle new clients without blocking other operations. This is a highly scalable and standard model for network servers.

-----

## Metadata Organization

* **Data Structures:** All system metadata is stored in-memory on the trackers for fast access.

    * `map<string, string> users`: Stores user credentials. A map provides O(log n) lookup time.
    * `map<string, set<string>> groups`: Stores group members. A `set` is used for the member list to automatically handle duplicate entries and provide fast lookups.
    * Other `map` structures are used for group admins, join requests, and managing login sessions (`session_by_fd`).

* **Thread Safety:** All access to these shared data structures is protected by a single, coarse-grained `pthread_mutex_t`.

* **Justification:** This single-mutex approach is simple to implement and reason about, and it guarantees that the state remains consistent. While a more fine-grained locking mechanism (e.g., a mutex per data structure) could offer higher performance under extreme load, it would significantly increase complexity. For the scope of this project, a single mutex provides the required safety with acceptable performance.