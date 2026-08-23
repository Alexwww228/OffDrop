# OffDrop

**Peer-to-peer file transfer over Bluetooth — no Wi-Fi, router, or internet required.**

OffDrop is an experimental cross-platform file transfer system built around a **Bluetooth-only transport layer**.

The project separates the file-transfer protocol, cryptography, and data processing from the underlying transport and platform APIs. This makes the core independently testable and allows different Bluetooth implementations to be plugged in without changing the transfer protocol itself.

The current implementation consists of a **platform-independent C++17 core** with a **Go daemon connected through a C API and cgo**.

> **Status:** Experimental / Work in Progress

<img width="1012" height="664" alt="Screenshot 2026-08-23 at 15 11 07" src="https://github.com/user-attachments/assets/9d33ae2f-a1d6-49f1-9b58-3ad89ad7c4ad" />

---

## Why OffDrop?

Most modern device-to-device file transfer solutions rely on Wi-Fi, a local network, or a combination of wireless technologies.

OffDrop explores a different approach:

> **Can a reliable and encrypted peer-to-peer file transfer system be built with Bluetooth as the only transport?**

The main engineering challenge is not simply sending bytes over Bluetooth. The project focuses on building a transport-independent protocol capable of handling:

* connection handshakes;
* peer authentication and key exchange;
* encrypted file transfer;
* chunking and reassembly;
* out-of-order delivery;
* integrity verification;
* transport abstraction;
* platform-specific transport implementations.

---

## Key Features

* 📡 **Bluetooth-only transport**
* 🔐 **End-to-end encrypted file transfer**
* 🔑 **X25519 ephemeral key exchange**
* 🧬 **HKDF-SHA256 key derivation**
* 🔒 **AES-256-GCM authenticated encryption**
* 🧩 **Chunk-based file transfer**
* 🔀 **Out-of-order chunk reassembly**
* ✅ **SHA-256 file integrity verification**
* 🧪 **Loopback transport for deterministic testing**
* 🧱 **Platform-independent C++17 core**
* 🔌 **C API for language interoperability**
* 🐹 **Go ↔ C++ integration through cgo**
* 🖥️ **Platform-specific transport layer**

---

# Architecture

OffDrop is divided into three major layers:

```text
┌───────────────────────────────────────────────┐
│                  Go Daemon                    │
│                                               │
│              Application layer                │
└───────────────────────┬───────────────────────┘
                        │
                     cgo / C API
                        │
                        ▼
┌───────────────────────────────────────────────┐
│                 C++ Core                      │
│                                               │
│  ┌────────────┐  ┌────────────┐              │
│  │  Protocol  │  │   Crypto   │              │
│  └────────────┘  └────────────┘              │
│                                               │
│  ┌────────────┐  ┌─────────────────────────┐  │
│  │  Chunking  │  │   File Transfer Logic   │  │
│  └────────────┘  └─────────────────────────┘  │
│                                               │
│              ITransport abstraction           │
└───────────────────────┬───────────────────────┘
                        │
                        ▼
┌───────────────────────────────────────────────┐
│              Platform Layer                   │
│                                               │
│  macOS / iOS          Windows                 │
│  CoreBluetooth        C++/WinRT               │
└───────────────────────────────────────────────┘
```

### Core principle

The C++ core does **not** know how data is physically transmitted.

Instead, transport-specific functionality is hidden behind the `ITransport` interface:

```cpp
class ITransport {
public:
    virtual ~ITransport() = default;

    virtual bool send(const uint8_t* data, size_t size) = 0;
    virtual bool receive(uint8_t* data, size_t size) = 0;
};
```

This allows the same protocol and file-transfer implementation to work with different transports.

For development and testing, OffDrop currently provides a `LoopbackTransport`, which allows two peers to communicate entirely in memory without requiring Bluetooth hardware.

---

# Transfer Pipeline

A file transfer follows a pipeline similar to:

```text
File
 │
 ▼
Chunking
 │
 ▼
Protocol messages
 │
 ▼
Encryption
 │
 ▼
Transport
 │
 ▼
Decryption
 │
 ▼
Chunk reassembly
 │
 ▼
SHA-256 verification
 │
 ▼
File
```

Files are divided into independently transferable chunks.

Chunks contain enough metadata for the receiver to reconstruct the original file even when chunks arrive out of order.

After all chunks have been received, the resulting file is verified against its SHA-256 digest.

---

# Protocol

OffDrop uses a binary protocol implemented independently from the underlying transport.

The protocol currently supports:

```text
Handshake
    │
    ▼
Key exchange
    │
    ▼
Session establishment
    │
    ▼
File offer
    │
    ▼
Chunk transfer
    │
    ▼
Transfer completion
    │
    ▼
Integrity verification
```

The protocol layer is responsible for describing **what** is being transmitted, while the transport layer is responsible for **how** those bytes are delivered.

This separation allows the protocol to be tested without depending on Bluetooth APIs.

---

# Security

OffDrop uses modern cryptographic primitives for establishing encrypted sessions and protecting transferred data.

| Primitive       | Purpose                     |
| --------------- | --------------------------- |
| **X25519**      | Ephemeral key exchange      |
| **HKDF-SHA256** | Session key derivation      |
| **AES-256-GCM** | Authenticated encryption    |
| **SHA-256**     | File integrity verification |

The cryptographic layer is implemented inside the platform-independent C++ core.

### Encryption flow

```text
Peer A                         Peer B
  │                              │
  │──── X25519 public key ──────>│
  │<──── X25519 public key ──────│
  │                              │
  │       Shared secret          │
  │───────────┬──────────────────│
              │
              ▼
        HKDF-SHA256
              │
              ▼
        Session key
              │
              ▼
        AES-256-GCM
              │
              ▼
        Encrypted data
```

> **Security note:** OffDrop is an experimental project and has not undergone an independent security audit. It should not currently be considered suitable for protecting highly sensitive data.

---

# Project Structure

```text
.
├── core/
│   ├── protocol.*          # Binary protocol
│   ├── crypto.*            # Key exchange and encryption
│   ├── chunking.*          # File chunking and reassembly
│   ├── transport.hpp       # Transport abstraction
│   ├── c_api.*             # C API exposed to Go
│   └── tests/              # Core tests
│
├── cmd/
│   └── daemon/
│       ├── bridge.go       # Go ↔ C++ bridge
│       └── main.go         # Go daemon
│
├── platform/
│   └── macos/
│       └── ...             # CoreBluetooth integration
│
├── CMakeLists.txt
└── README.md
```

---

# Testing

One of the core design goals is keeping the transfer protocol testable without physical Bluetooth hardware.

The `LoopbackTransport` provides an in-memory implementation of `ITransport`:

```text
┌──────────────┐             ┌──────────────┐
│    Peer A    │             │    Peer B    │
│              │             │              │
│ C++ Core     │             │ C++ Core     │
└──────┬───────┘             └──────▲───────┘
       │                            │
       └──── LoopbackTransport ─────┘
```

This makes it possible to test:

* protocol messages;
* handshake;
* encryption/decryption;
* chunking;
* out-of-order delivery;
* file reconstruction;
* integrity verification;

without depending on Bluetooth availability or platform-specific APIs.

---

# Build

## Requirements

* CMake
* C++17 compiler
* OpenSSL 3
* Go
* macOS with CoreBluetooth support for the current platform integration

---

## Build the C++ Core

```bash
mkdir build
cd build

cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
```

---

## Run Tests

```bash
./core/tests/airdrop_core_tests
```

The test suite runs against the platform-independent core and does not require Bluetooth hardware.

---

## Build the Go Daemon

From `cmd/daemon`:

```bash
CGO_ENABLED=1 \
CGO_LDFLAGS="-L/opt/homebrew/opt/openssl@3/lib -L../../build/core" \
CGO_CFLAGS="-I/opt/homebrew/opt/openssl@3/include" \
go build -o ../../build/daemon .
```

Run:

```bash
../../build/daemon
```

---

# Current Status

### Core

* [x] Binary protocol
* [x] Handshake
* [x] Key exchange
* [x] File offers
* [x] Chunk-based transfer
* [x] Out-of-order chunk assembly
* [x] AES-256-GCM encryption
* [x] SHA-256 integrity verification
* [x] Loopback transport
* [x] C API
* [x] Go ↔ C++ integration through cgo

### Platform Integration

* [ ] CoreBluetooth transport
* [ ] macOS menu-bar application
* [ ] iOS client
* [ ] Windows C++/WinRT transport

---

# Roadmap

### Phase 1 — Core

* [x] Design binary protocol
* [x] Implement handshake
* [x] Implement encrypted sessions
* [x] Implement chunked transfers
* [x] Implement integrity verification
* [x] Implement transport abstraction
* [x] Add loopback testing

### Phase 2 — macOS

* [ ] Implement CoreBluetooth transport
* [ ] Connect CoreBluetooth to `ITransport`
* [ ] Test real device-to-device transfers
* [ ] Build macOS menu-bar application

### Phase 3 — Mobile

* [ ] iOS client
* [ ] Device discovery
* [ ] Background transfer support where possible

### Phase 4 — Windows

* [ ] C++/WinRT Bluetooth transport
* [ ] Windows client
* [ ] Cross-platform interoperability testing

---

# Engineering Goals

OffDrop is primarily an exploration of **transport-independent systems design**.

The project focuses on several engineering problems:

* designing a binary application protocol;
* separating protocol logic from transport implementation;
* implementing encrypted sessions;
* handling unreliable or out-of-order data delivery;
* designing deterministic tests for transport-dependent systems;
* integrating C++ and Go through a stable C ABI;
* keeping platform-specific code isolated from the core.

The long-term goal is to make the same transfer protocol operate across different platforms and Bluetooth implementations without modifying the core file-transfer logic.

---

# Tech Stack

**Core**

* C++17
* CMake
* OpenSSL

**Application**

* Go
* cgo

**Platform**

* CoreBluetooth
* C++/WinRT *(planned)*

**Cryptography**

* X25519
* HKDF-SHA256
* AES-256-GCM
* SHA-256

---

# License

This project is currently experimental and intended for educational and research purposes.
