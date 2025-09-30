# Peer-to-Peer File Sharing System

## Overview
This project is a peer-to-peer (P2P) file sharing system in C++. It uses two trackers (like coordinators) for redundancy so that even if one goes down, the system still works. The trackers only handle metadata (users, groups, files), while the actual file transfer happens directly between clients.

---

## Compilation and Execution

### Compilation

```bash
make clean && make
```

This will produce two executables: `tracker.out` and `client.out`.

### Running the System


**1. Start Tracker 1 (Terminal 1)**

```bash
./tracker.out tracker_info.txt 1
```

It will start listening. Don’t worry if you see a "Failed to connect to peer" message — that’s expected at first.

**2. Start Tracker 2 (Terminal 2)**

```bash
./tracker.out tracker_info.txt 2
```

This will connect to Tracker 1. You’ll see "Handshake received" on Tracker 1, meaning they’re now synced.

**3. Start the Client (Terminal 3)**

```bash
./client.out 127.0.0.1:9909 tracker_info.txt
```

The client connects to whichever tracker is alive and ready. From here you can start issuing commands.

-----


## Synchronization Approach

- Whenever a client does something that changes the system state (like create_user or accept_request), the tracker that got the command updates itself first. Then it sends a SYNC command to the other tracker so both have the same data.

- When the second tracker comes online, it sends a SYNC_INIT to make sure both trackers connect to each other.

- This is basically a primary-update model: one tracker applies the change, then tells the other.
It’s simple, fast, and avoids the complexity of heavyweight consensus algorithms like Paxos or Raft (which are unnecessary for just two nodes).

-----

## Handling New Connections

- When a new client connects, the tracker uses select() to notice the incoming connection. Then it calls accept(), creates a new socket for that client, and adds it to the monitored set. This way, the tracker can handle many clients at once without blocking.

-----

## Metadata Organization

- **Data Structures:** All system metadata is stored in-memory on the trackers for fast access.

    - `map<string, string> users`: Stores user credentials.
    - `map<string, set<string>> groups`: Stores group members.
    - Other maps for things like group admins, join requests, and active sessions

- Everything is guarded by a mutex for safety.