<div align="center">

# 📡 UniSync

### AirDrop for the campus lab

Discover nearby devices and send files or whole folders directly over the LAN.<br>
**No cloud. No USB. No accounts.**

[![C](https://img.shields.io/badge/Language-C%20(GNU%20C11)-00599C?logo=c&logoColor=white)](#-tech-stack)
[![Platform](https://img.shields.io/badge/Platform-Linux-FCC624?logo=linux&logoColor=black)](#-build--deploy)
[![Network](https://img.shields.io/badge/Network-TCP%20%2B%20UDP-blue)](#-build--deploy)
[![Security](https://img.shields.io/badge/Security-SHA--256%20%7C%20ChaCha20-success)](#-features)
[![Tests](https://img.shields.io/badge/Tests-30%20end--to--end-brightgreen)](#-build--deploy)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](#-license)
[![Version](https://img.shields.io/badge/Version-2.0-orange)](#-status)

</div>

```bash
# 🖥️ Computer A (receiver)           # 💻 Computer B (sender)
./bin/UniSync receive                ./bin/UniSync send notes.pdf
                                     # → pick the device → A answers  y / n / a
```

---

## 📖 Table of Contents

[Concept](#-concept) · [Quick Start](#-quick-start) · [Commands](#-all-commands) · [Security](#-protect-it-with-a-pin-and-encryption) · [Features](#-features) · [Tech Stack](#-tech-stack) · [Structure](#-project-structure) · [Build & Deploy](#-build--deploy) · [Design](#-design-system) · [Customize](#-customize) · [Limitations](#-limitations) · [License](#-license) · [Status](#-status)

---

## 💡 Concept

UniSync solves a simple campus problem: **sharing files between lab machines on the same Wi-Fi** without the internet, pendrives, or third-party apps.

| Step | What happens |
|:---:|---|
| **1** | The receiver starts (`UniSync receive`) and announces itself on the network |
| **2** | The sender runs `UniSync send <file or folder>` and picks a device from the list |
| **3** | The receiver sees who is sending what and answers **y**es / **n**o / **a**lways trust |
| **4** | Files stream over a direct TCP connection with a live progress bar |
| **5** | Every file is checked with SHA-256 before it is saved; interrupted transfers resume |

> 🔒 Everything stays on the LAN. Nothing leaves the room.

---

## 🚀 Quick Start

```bash
sudo apt update && sudo apt install -y build-essential git
git clone https://github.com/parim2250/UniSync.git
cd UniSync
make
```

### 📤 Send a file

Two terminals. Works on one machine too.

```bash
# Terminal 1 — start the receiver ("server")
./bin/UniSync receive

# Terminal 2 — send
./bin/UniSync send notes.pdf                    # finds the receiver automatically
./bin/UniSync send photos/ report.pdf           # folders and several files at once
./bin/UniSync send big.iso --to 192.168.1.20    # or give the IP yourself
```

Terminal 1 now shows:

```text
  ┌─ Incoming transfer ──────────────────
  │ From    : MyLaptop (192.168.1.21)
  │ Sending : notes.pdf
  │ Size    : 1 file(s), 2.3 MB
  │ Security: PIN off   Encryption off
  └──────────────────────────────────────
  Accept? [y]es / [n]o / [a]lways trust this device:
```

| Key | Meaning |
|:---:|---|
| `y` | Accept this transfer |
| `n` | Reject (the sender is told) |
| `a` | Accept and remember the device, so it is never asked again |

📁 Received files go to `~/UniSync_Received` (change with `--dir`).

---

## 🧰 All Commands

| Command | What it does |
|---|---|
| `UniSync receive` | Start receiving. Options: `--dir FOLDER` `--pin 1234` `--encrypt` `--auto-accept` `--port N` `--name NAME` `--max-size MB` `--exec PROGRAM` |
| `UniSync send <paths>...` | Send files/folders. Options: `--to IP[:PORT]` `--pin 1234` `--encrypt` |
| `UniSync devices` | List UniSync devices on the network |
| `UniSync history [n]` | Past transfers (time, direction, file, device, status, size) |
| `UniSync paired` | Show paired (trusted) and blocked devices |
| `UniSync pair <name>` / `unpair <name>` | Trust / forget a device |
| `UniSync block <name>` / `unblock <name>` | Always refuse / allow a device |
| `UniSync selftest` | Check SHA-256 and ChaCha20 against official test vectors |

> ℹ️ The old command names still work: `UniSync-file-receiver [port] [dir]`, `UniSync-file-sender <file> [ip] [port]`, `UniSync-discover`.

---

## 🔐 Protect it with a PIN (and encryption)

```bash
./bin/UniSync receive --pin 4321                 # receiver needs a PIN from senders
./bin/UniSync send notes.pdf                     # sender is asked for the PIN (typed hidden)
./bin/UniSync send notes.pdf --pin 4321          # or pass it directly

./bin/UniSync receive --pin 4321 --encrypt       # require encrypted transfers
./bin/UniSync send notes.pdf --pin 4321          # sender switches to encryption automatically
```

<details>
<summary><b>🔁 Demo: resume after a dropped connection</b></summary>

<br>

```bash
# Terminal 1
./bin/UniSync receive --auto-accept

# Terminal 2
head -c 50000000 /dev/urandom > /tmp/big.bin
./bin/UniSync send /tmp/big.bin --to 127.0.0.1 --simulate-drop 20000000
```

The sender cuts its own connection after 20 MB, reconnects, and the receiver prints `Resuming big.bin from ... bytes`.

</details>

---

## ✨ Features

| | Feature | Description |
|:---:|---|---|
| 🔍 | **Device discovery** | Receivers answer UDP broadcast **and multicast** (239.255.42.99) "who is there?" requests |
| ⚡ | **Direct file transfer** | Chunked TCP transfer (64 KB blocks), never loads a whole file into RAM |
| 📂 | **Files and folders** | Recursive folder transfer, empty folders/files preserved, several items in one go |
| ✅ | **Accept / Reject / Always** | Receiver confirms every incoming transfer; `a` pairs the device |
| 📊 | **Live progress** | Bar, percent, MB transferred, speed and ETA |
| 🧵 | **Concurrent transfers** | One pthread per incoming connection, semaphore limits simultaneous transfers |
| 🔄 | **Interrupted-transfer recovery** | Partial data is kept; on reconnect the receiver tells the sender where to continue. Sender retries automatically |
| 🧮 | **Integrity verification** | SHA-256 (implemented from scratch) checked by the receiver before the file is saved |
| 🛡️ | **Security** | PIN challenge/response (HMAC-SHA256, PIN never sent), pairing, block list, size limit, path-traversal protection |
| 🔒 | **Optional encryption** | ChaCha20 (RFC 8439, from scratch), session key derived from the PIN |
| 🔑 | **Permission preserve** | Source `chmod` mode is sent and restored |
| 🕘 | **History & logging** | `UniSync history` + timestamped `~/.unisync/unisync.log` |
| 🪝 | **Post-receive hook** | `--exec PROGRAM` runs a program (fork/exec/waitpid) for each received file |
| 🏷️ | **Transfer metadata** | File name, size, type, transfer ID, checksum and chunk size are part of the protocol |
| 🧼 | **Input validation** | Friendly errors for missing files, bad IPs/ports, unwritable folders, busy ports |

---

## 🛠️ Tech Stack

| Layer | Technology |
|---|---|
| **Language** | C (GNU C11) |
| **Networking** | POSIX sockets: TCP (transfer) + UDP (discovery) |
| **Concurrency** | POSIX threads, mutexes, semaphores |
| **File I/O** | `open` `read` `write` `lseek` `ftruncate` `fsync` `rename` `stat` `chmod` |
| **Processes & signals** | `fork` `execlp` `waitpid`, `sigaction` |
| **Build** | GNU Make + GCC |
| **Platform** | Linux (tested on Ubuntu) |

> 🎓 Matches the Unix course syllabus: file descriptors, sockets, threads, processes and real system calls, not wrappers.

---

## 📁 Project Structure

```text
UniSync/
├── include/                 # Public headers
│   ├── common.h             # settings, constants, colours
│   ├── network.h            # TCP helpers + framed, encryptable socket I/O
│   ├── protocol.h           # wire protocol v2
│   ├── transfer.h           # folder walking, chunked send/receive, resume
│   ├── discovery.h          # UDP/multicast discovery
│   ├── progress.h           # live progress bar
│   ├── security.h           # pairing / block list
│   ├── sha256.h  crypto.h   # SHA-256 + HMAC, ChaCha20
│   ├── history.h  logger.h  # history + log
│   ├── handler.h            # per-client receiver thread
│   └── ui.h  util.h  errors.h  app.h
│
├── src/
│   ├── main.c               # the `UniSync` command (receive / send / devices / history ...)
│   ├── file_receiver.c      # `UniSync receive`
│   ├── file_sender.c        # `UniSync send` (device picker, retry + resume)
│   ├── discover_main.c      # `UniSync devices`
│   ├── handler.c            # receiver session: accept prompt, files, verification
│   ├── protocol.c           # handshake (PIN, encryption) + message framing
│   ├── transfer.c           # file engine
│   ├── discovery.c  network.c  progress.c  security.c  history.c  logger.c
│   ├── sha256.c  crypto.c  ui.c  util.c  errors.c
│   ├── server.c / client.c  # Layer-1 socket demos (unchanged)
│   └── thread_pool.c/.h     # earlier thread-pool prototype (not used by the build)
│
├── tests/run_tests.sh       # 30 end-to-end checks  (make test)
└── Makefile  README.md  .gitignore
```

---

## 🏗️ Build & Deploy

```bash
make                 # build everything into bin/
make test            # run the end-to-end tests (needs python3)
make clean
sudo make install    # optional: copies UniSync to /usr/local/bin
```

| Service | Port | Protocol |
|---|:---:|---|
| File transfer | `9876` (change with `--port`) | TCP |
| Device discovery | `9877` | UDP broadcast / multicast |

> 🔥 **Firewall?** `sudo ufw allow 9876/tcp && sudo ufw allow 9877/udp`
>
> 📶 Some campus Wi-Fi networks stop devices from seeing each other. In that case use `--to IP`.

🗃️ Data is kept in `~/.unisync/` (`history.log`, `unisync.log`, `paired.txt`, `blocked.txt`).

---

## 🧩 Design System

### Architecture

```text
┌──────────────────────┐   UDP broadcast/multicast   ┌──────────────────────┐
│  UniSync send        │ ──────  "who is there?" ──► │  UniSync receive     │
│  (device picker)     │ ◄──── name, port, flags ─── │  (discovery thread)  │
└─────────┬────────────┘                             └─────────┬────────────┘
          │  TCP: handshake (PIN / encryption) → offer         │
          │  ─────────────────────────────────────────────►    │ one thread per client
          │  ◄─────────────── accept / reject (y/n/a) ──────    │
          │  per file: header + SHA-256 ► ◄ resume offset      │
          │  ───────── data chunks ───────────────────────►    │ verify SHA-256 → save
          │  ◄─────────────── verified ✔ ───────────────────    │ history + log
          ▼                                                     ▼
```

### Wire protocol v2

```text
S→R  "UNSY", version, senderName
R→S  flags (PIN required / encryption required), 16-byte nonce
S→R  encrypt?, HMAC-SHA256(PIN, nonce)            ← the PIN itself is never sent
R→S  status, receiverName          (if encrypting: both sides switch to ChaCha20 here)
S→R  offer: nfiles, totalBytes, label, transferID, chunkSize
R→S  accept / reject
per item:  'D' folder, mode
           'F' path, fileType, size, mode, SHA-256 → R→S resume offset → data → R→S verified
S→R  'E'
```

### Core design rules

- 🧱 **Never load an entire file into memory.** Fixed chunk buffer only
- 📎 **Everything is a file descriptor.** Sockets and files use the same I/O model
- 📚 **Headers are the API.** Modules talk through `include/*.h`
- 🚨 **Fail loud, don't crash.** Validate paths/IPs/ports before socket work
- 🧾 **Safe saves.** A file only appears under its real name after its SHA-256 matches (`name.unisync.part` until then)

### Concurrency model

- Receiver main thread: `accept()` loop only
- One detached pthread per client; a semaphore limits simultaneous transfers
- A mutex makes sure only one thread asks y/n at a time; another protects the console and the log
- Discovery responder runs in its own thread

---

## 🎛️ Customize

| Knob | Where | Default |
|---|---|---|
| TCP port | `--port` / `DEFAULT_PORT` | `9876` |
| UDP discovery port | `discovery.h` | `9877` |
| Chunk size | `transfer.h` (`CHUNK_SIZE`) | `65536` |
| Progress bar width | `progress.c` (`BAR_WIDTH`) | `28` |
| Receive folder | `--dir` | `~/UniSync_Received` |
| Data / log folder | `main.c` (`cfg.datadir`) | `~/.unisync` |
| Reconnect attempts | `common.h` (`MAX_RETRIES`) | `6` |

<details>
<summary><b>🔌 Use discovery from your own tool</b></summary>

<br>

```c
#include "discovery.h"
DeviceInfo devices[MAX_DEVICES];
int n = discover_devices(devices, MAX_DEVICES, 2);
```

</details>

---

## ⚠️ Limitations

- Linux / IPv4 only
- Encryption uses a key derived from the shared PIN (confidentiality); integrity is guaranteed by the SHA-256 check. No public-key exchange, so use a strong PIN
- Terminal interface only

---

## 🤝 Connect

- **GitHub Repo:** [github.com/parim2250/UniSync](https://github.com/parim2250/UniSync)
- **Issues / Bugs:** use GitHub Issues

---

## 📄 License

Released under the **MIT License**. © 2026 Pari Mittal / UniSync Team

<details>
<summary>Read the full license text</summary>

<br>

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

</details>

> 🎓 *Academic note: built as a Unix / Systems Programming semester project (JIIT Noida, CSE).*

---

## 📈 Status

**v2.0 ✅ all synopsis features implemented**

- [x] TCP foundation + chunked file engine + wire protocol
- [x] Progress bar, error handling, logging
- [x] UDP discovery (broadcast + multicast) with receiver responder
- [x] Accept / Reject / Always flow, pairing, block list
- [x] Multithreaded receiver + semaphore limits
- [x] Folder and multi-file transfer
- [x] Resume of interrupted transfers
- [x] SHA-256 integrity verification
- [x] PIN authentication, optional ChaCha20 encryption
- [x] History, post-receive hook (fork/exec)
- [x] 30 automated end-to-end tests

<div align="center">

⭐ If UniSync helped you, consider starring the repo.

</div>