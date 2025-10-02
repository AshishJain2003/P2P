# Peer-to-Peer File Sharing System

A distributed file sharing system with dual-tracker architecture, multi-threaded parallel downloads, and automatic failover capabilities.

---

## Table of Contents
1. [System Overview](#system-overview)
2. [Features](#features)
3. [Architecture](#architecture)
4. [Compilation](#compilation)
5. [Running the System](#running-the-system)
6. [Commands](#commands)
7. [Implementation Details](#implementation-details)
8. [Testing](#testing)

---

## System Overview

This is a peer-to-peer file sharing system where users can:
- Create groups and manage memberships
- Share files within groups
- Download files from multiple peers simultaneously
- Benefit from automatic tracker failover for reliability

The system uses a dual-tracker architecture for redundancy, with all state synchronized between trackers in real-time.

---

## Features

### Core Functionality
- **User Management:** Registration, login/logout with session management
- **Group Management:** Create groups, join/leave groups, accept/reject requests
- **File Sharing:** Upload files to groups, download from multiple peers
- **Parallel Downloads:** 4 worker threads per file for concurrent piece downloads
- **Load Balancing:** Random peer selection distributes load evenly
- **File Integrity:** SHA-1 verification ensures downloaded files are not corrupted
- **Progress Tracking:** Real-time download progress with `show_downloads` command

### Reliability Features
- **Dual Trackers:** Two synchronized trackers for redundancy
- **Automatic Failover:** Client reconnects to alternate tracker on failure
- **State Synchronization:** All tracker state changes immediately replicated
- **Authorization:** Group-based access control for files

---

## Architecture

### Tracker
- **Type:** Single-threaded event-driven server
- **I/O Model:** `select()` for non-blocking multiplexed I/O
- **Synchronization:** Active-active replication with command forwarding
- **State:** All metadata stored in-memory with mutex protection

### Client
- **Main Thread:** Interactive CLI for user commands
- **Seeder Thread:** Handles incoming piece requests from other peers
- **Download Threads:** One manager thread per file, spawns 4 worker threads
- **Protocol:** Custom length-prefixed TCP protocol

### File Structure
```
project/
├── tracker/
│   ├── main.cpp
│   ├── commands.cpp
│   ├── network.cpp
│   ├── state.cpp
│   └── *.h
├── client/
│   ├── main.cpp
│   ├── commands.cpp
│   ├── network.cpp
│   └── *.h
├── sha1.cpp
├── sha1.h
├── Makefile
├── tracker_info.txt
└── README.md
└── PerformanceReport.md
```

---

## Compilation

### Prerequisites
- **OS:** Linux (Ubuntu/Debian recommended)
- **Compiler:** g++ with C++11 support
- **Libraries:** pthread (standard on Linux)

### Build Commands
```bash
# Clean previous builds
make clean

# Compile everything
make

# This creates:
# - tracker.out
# - client.out
```

### Manual Compilation (if Makefile doesn't work)
```bash
# Tracker
g++ -std=c++11 -pthread tracker/*.cpp sha1.cpp -o tracker.out

# Client
g++ -std=c++11 -pthread client/*.cpp sha1.cpp -o client.out
```

---

## Running the System

### 1. Prepare Tracker Info File

Create `tracker_info.txt` with both tracker addresses:
```
127.0.0.1:9909
127.0.0.1:9910
```

### 2. Start Trackers

**Terminal 1 - Tracker 1:**
```bash
./tracker.out tracker_info.txt 1
```

**Terminal 2 - Tracker 2:**
```bash
./tracker.out tracker_info.txt 2
```

Wait for connection message: `Connected to peer tracker`

### 3. Start Clients

**Terminal 3 - Client 1:**
```bash
./client.out 127.0.0.1:9909 tracker_info.txt
```

**Terminal 4 - Client 2:**
```bash
./client.out 127.0.0.1:9910 tracker_info.txt
```

Clients automatically register their seeder port after login.

---

## Commands

### User Management
```bash
create_user <user_id> <password>     # Register new user
login <user_id> <password>           # Login to system
logout                               # Logout and stop sharing files
```

### Group Management
```bash
create_group <group_id>              # Create a new group (you become owner)
join_group <group_id>                # Request to join a group
leave_group <group_id>               # Leave a group
list_groups                          # Show all available groups
list_requests <group_id>             # Show pending join requests (owner only)
accept_request <group_id> <user_id>  # Accept join request (owner only)
```

### File Operations
```bash
upload_file <group_id> <file_path>           # Share file with group
list_files <group_id>                        # List files in group
download_file <group_id> <filename> <dest>   # Download file from group
show_downloads                               # Show download progress
stop_share <group_id> <filename>             # Stop sharing a file
```

### System Commands
```bash
exit        # Exit client
quit        # Shutdown tracker (from tracker console)
```

---

## Implementation Details

### File Transfer
- **Piece Size:** 512 KB (524,288 bytes)
- **Hashing:** SHA-1 for each piece
- **Verification:** Each piece verified before writing to disk
- **Parallelism:** 4 worker threads download different pieces simultaneously

### Peer Selection
- **Algorithm:** Random selection from available peers
- **Load Balancing:** Statistical distribution ensures even load
- **Failover:** Automatic retry with different peer on failure

### Tracker Synchronization
- **Model:** Active-active with immediate replication
- **Protocol:** `SYNC` commands forwarded to peer tracker
- **Consistency:** Strong consistency - both trackers identical

### Network Protocol
- **Message Format:** `[4-byte length][payload]`
- **Transport:** TCP with custom framing
- **Error Handling:** Automatic reconnection on failure

### Thread Safety
- **Mutexes:** Protect all shared data structures
- `state_mutex`: Tracker state
- `client_mutex`: Tracker socket
- `pieces_mutex`: Piece queue
- `downloads_mutex`: Download progress
- `cout_mutex`: Console output

---

## Limitations and Assumptions

### Current Limitations
- **No Persistence:** All data lost on tracker restart
- **No Encryption:** Data transmitted in plaintext
- **Local Network:** Optimized for LAN, not tested on WAN
- **Fixed Thread Pool:** 4 threads per download (not adaptive)

### Assumptions
- **Trusted Network:** All peers are trustworthy
- **Reliable Tracker Connection:** Trackers can communicate reliably
- **File System:** Sufficient disk space for downloads
- **Unique Filenames:** No duplicate filenames within a group

---