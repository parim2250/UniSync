# UniSync

**AirDrop for the campus lab.**

A local-network file sharing tool for Linux that lets students discover nearby devices and transfer files directly over LAN — no cloud, no USB, no setup accounts.

```bash
./bin/UniSync-discover          # find devices on Wi-Fi
./bin/UniSync-file-sender notes.pdf
# peer gets Accept/Reject → progress bar → done
```

## Concept

UniSync solves a simple campus problem: sharing files between lab machines on the same Wi-Fi without the internet, pendrives, or third-party apps.

It works like AirDrop, but for Linux terminals:

1. Discover peers on the local network via UDP broadcast
2. Select a device from the list
3. Send a file over a direct TCP connection
4. Accept / Reject on the receiver side
5. Verify transfer with progress, permissions, and logs

Everything stays on the LAN. Nothing leaves the room.

## Features

| Feature | Description |
|---|---|
| Device Discovery | UDP broadcast finds UniSync peers on the same Wi-Fi in ~3 seconds |
| Direct File Transfer | Chunked TCP transfer (64KB blocks) — never loads whole file into RAM |
| Wire Protocol | Structured header (magic, version, filename, size, permissions) before payload |
| Accept / Reject | Receiver confirms every incoming file |
| Live Progress Bar | Percentage, MB transferred, and speed (MB/s) |
| Multithreaded Receiver | Handle multiple incoming transfers at once (pthread) |
| Permission Preserve | Source chmod mode is sent and restored on the receiver |
| Input Validation | Friendly errors for missing files, bad IPs, bad ports, unwritable dirs |
| Logging | Timestamped events in `logs/unisync.log` |
| Zero Cloud | Fully offline, LAN-only |

## Tech Stack

| Layer | Technology |
|---|---|
| Language | C (C11) |
| Networking | POSIX sockets — TCP + UDP |
| Concurrency | POSIX threads (pthread) |
| File I/O | Unix `open`/`read`/`write`/`close`, `stat`, `chmod` |
| Build | GNU Make + GCC |
| Platform | Linux (tested on Ubuntu) |
| Version control | Git + GitHub |

### Why C + POSIX?

Matches the Unix course syllabus: file descriptors, sockets, processes/threads, and real system calls — not wrappers.

## Project Structure

```text
UniSync/
├── include/                 # Public headers (stable API for teammates)
│   ├── network.h            # TCP socket helpers
│   ├── protocol.h           # Wire protocol + FileHeader
│   ├── transfer.h           # Chunked file I/O helpers
│   ├── discovery.h          # UDP LAN discovery
│   ├── progress.h           # Live progress bar
│   ├── errors.h             # Validation + error strings
│   ├── handler.h            # Per-client thread handler
│   └── logger.h             # Timestamped file logging
│
├── src/                     # Implementations
│   ├── network.c
│   ├── protocol.c
│   ├── transfer.c
│   ├── discovery.c
│   ├── progress.c
│   ├── errors.c
│   ├── handler.c
│   ├── logger.c
│   ├── file_sender.c        # Interactive sender CLI
│   ├── file_receiver.c      # Multithreaded receiver CLI
│   ├── discover_main.c      # Discovery CLI
│   ├── server.c / client.c  # Layer-1 socket demos
│   └── main.c               # Entry banner / usage
│
├── bin/                     # Built binaries (gitignored)
├── obj/                     # Object files (gitignored)
├── logs/                    # Runtime logs (gitignored)
├── downloads/               # Default receive folder (optional)
├── tests/                   # Test assets / future unit tests
├── Makefile
├── README.md
└── .gitignore
```

## Quick Start

### Requirements

- Linux (Ubuntu 22.04+ recommended)
- `gcc`, `make`, `build-essential`
- Same Wi-Fi / LAN for multi-device demos

### Install tools

```bash
sudo apt update
sudo apt install build-essential git
```

### Clone & build

```bash
git clone git@github.com:parim2250/UniSync.git
cd UniSync
make clean && make
```

### One-minute demo (same machine)

```bash
# Terminal 1 — receiver
mkdir -p downloads
./bin/UniSync-file-receiver 9876 downloads

# Terminal 2 — sender
echo "hello from UniSync" > demo.txt
./bin/UniSync-file-sender demo.txt 127.0.0.1 9876
# switch to Terminal 1 → press y
```

### LAN demo (two machines)

```bash
# Machine A
./bin/UniSync-file-receiver 9876 ~/Downloads

# Machine B
./bin/UniSync-discover
./bin/UniSync-file-sender report.pdf
# pick Machine A from the list → accept on A
```

## Build & Deploy

### Build

```bash
make            # build all binaries
make clean      # remove bin/ and obj/
```

### Binaries produced

| Binary | Role |
|---|---|
| `bin/UniSync` | Usage banner |
| `bin/UniSync-discover` | LAN device scan |
| `bin/UniSync-file-sender` | Send file (discover or manual IP) |
| `bin/UniSync-file-receiver` | Receive files (threaded) |
| `bin/UniSync-server` | Low-level TCP server demo |
| `bin/UniSync-client` | Low-level TCP client demo |

### Run ports

| Service | Port | Protocol |
|---|---|---|
| File transfer | 9876 | TCP |
| Device discovery | 9877 | UDP broadcast |

### Deploy on a lab PC

```bash
make clean && make
sudo cp bin/UniSync* /usr/local/bin/    # optional system-wide
```

No daemon/service required — run receiver when you want to accept files.

## Design System

### Architecture (high level)

```text
┌──────────────────────┐       UDP broadcast        ┌──────────────────────┐
│  UniSync-discover /  │ ─────────────────────────► │  Other UniSync peers │
│  file_sender         │ ◄───────────────────────── │  on same LAN         │
└─────────┬────────────┘     device list (name/ip)  └─────────┬────────────┘
          │                                                   │
          │ TCP connect + FileHeader + payload                │
          ▼                                                   ▼
┌──────────────────────┐                            ┌──────────────────────┐
│  protocol + progress │                            │  file_receiver       │
│  send chunks         │ ────── ACCEPT/REJECT ────► │  handler thread      │
└──────────────────────┘                            │  chmod + save + log  │
                                                     └──────────────────────┘
```

### Wire protocol

Every transfer starts with a fixed, packed header:

```text
FileHeader (packed)
├── magic[4]      = "UNSY"
├── version       = 1
├── filename[256]
├── filesize      = uint64 (network byte order)
└── mode          = uint32 POSIX permissions
followed by exactly `filesize` payload bytes
```

### Core design rules

- Never load entire file into memory — fixed chunk buffer only
- Everything is a file descriptor — sockets and files use the same I/O model
- Headers are the API — teammates extend via new `.c` files, not by editing core
- Fail loud, don't crash — validate paths/IPs/ports before socket work
- LAN trust model — Accept/Reject today; PIN/SHA can be added by Person 4

### Concurrency model

- Receiver main thread: `accept()` loop only
- One pthread per client
- Mutex protects console prompts + log writes
- Threads are detached (auto cleanup)

## Section Map

| Section | What it covers |
|---|---|
| Concept | Problem + product idea |
| Features | User-visible capabilities |
| Tech Stack | Languages, libs, platform |
| Project Structure | Folders and responsibilities |
| Quick Start | Fastest path to first transfer |
| Build & Deploy | Compile, ports, lab install |
| Design System | Architecture, protocol, rules |
| Customize | How to extend safely |
| Connect via | Contact / repo links |
| License | Usage terms |

## Customize

UniSync is built for extension without breaking the core.

### Safe knobs

| Knob | Where | Default |
|---|---|---|
| TCP port | CLI arg / `DEFAULT_PORT` | 9876 |
| UDP discovery port | `discovery.h` | 9877 |
| Chunk size | `protocol.c` (`CHUNK_SIZE`) | 65536 |
| Progress bar width | `progress.c` (`BAR_WIDTH`) | 30 |
| Save directory | receiver CLI arg | `.` |
| Log path | `log_init("logs/unisync.log")` | `logs/unisync.log` |

### Extension guide (team)

| Person | Feature | Suggested approach |
|---|---|---|
| P3 | Resume interrupted transfers | Add offset field to header; use `lseek` + partial send/receive |
| P4 | Security | SHA-256 checksum in header + PIN challenge before ACCEPT |
| P5 | Advanced CLI / history | Wrapper CLI + parse `logs/unisync.log` for dashboard/history |

**Rule:** add new files + call existing headers. Do not silently rewrite `network.c` / `protocol.c` behavior.

### Example: call discovery from your own tool

```c
#include "discovery.h"

DeviceInfo devices[MAX_DEVICES];
int n = discover_devices(devices, MAX_DEVICES, 3);
```

## Connect via

- GitHub Repo: [github.com/parim2250/UniSync](https://github.com/parim2250/UniSync)
- Issues / Bugs: use GitHub Issues
- Team handoff: freeze headers under `include/` as the public API
- Demo tip: one receiver + two senders on the same LAN shows discovery, accept/reject, threads, and progress best

## License

```text
MIT License

Copyright (c) 2026 Pari Mittal / UniSync Team

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
```

*Academic note: built as a Unix / Systems Programming semester project. Core networking, protocol, and transfer engine authored by Person 1 + Person 2.*

## Status

```text
Core v1.0  ✅  complete
------------
[x] TCP foundation
[x] Chunked file engine
[x] Wire protocol
[x] Progress bar
[x] Error handling
[x] UDP discovery
[x] Accept/Reject flow
[x] Multithreaded receiver
[x] Permission preserve
[x] Logging
[x] Docs + cleanup
```

UniSync is ready for teammate feature layers and lab demos.

---

### Save & commit

```bash
git add README.md
git commit -m "add full project readme with architecture and quick start"
git push
```