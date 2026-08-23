# OffDrop

**Peer-to-peer file transfer over Bluetooth — no Wi-Fi, router, or internet required.**

OffDrop is an experimental cross-platform file transfer system that enables devices to transfer files directly over **Bluetooth**, without relying on Wi-Fi or an internet connection.

Unlike AirDrop and similar solutions such as LocalSend or Snapdrop, OffDrop is designed around **Bluetooth-only transport**.

## Features

* 📡 Bluetooth-only peer-to-peer file transfer
* 🔐 X25519 + HKDF-SHA256 + AES-256-GCM encryption
* 🧩 Chunk-based file transfer
* 🔀 Out-of-order chunk assembly
* ✅ SHA-256 file integrity verification
* 🧪 Fully testable core with a simulated `LoopbackTransport`
* 🌍 Platform-independent C++17 core
* 🔌 Go ↔ C++ integration via `cgo`

## Architecture

```text
                    ┌─────────────────────┐
                    │      Go daemon      │
                    │       cgo API       │
                    └──────────┬──────────┘
                               │
                               ▼
                    ┌─────────────────────┐
                    │      C++ Core       │
                    │                     │
                    │  Protocol           │
                    │  Crypto             │
                    │  Chunking           │
                    │  ITransport         │
                    └──────────┬──────────┘
                               │
                    ┌──────────▼──────────┐
                    │   Platform layer    │
                    │                     │
                    │   CoreBluetooth     │
                    │    macOS / iOS      │
                    └─────────────────────┘
```

### Project structure

```text
core/
├── protocol.*       # Binary protocol
├── crypto.*         # Cryptography
├── chunking.*       # File splitting and reassembly
├── transport.hpp    # Transport abstraction
├── c_api.*          # C API for cgo
└── tests/            # Core tests

cmd/daemon/
├── bridge.go        # Go ↔ C++ bridge
└── main.go          # Daemon

platform/macos/
└── CoreBluetooth bridge
```

The core is completely independent of Bluetooth.

All transport-specific functionality is isolated behind the `ITransport` interface. This allows the protocol, cryptography, and file-transfer logic to be developed and tested independently from the underlying platform.

## Security

OffDrop uses:

* **X25519** — ephemeral key exchange
* **HKDF-SHA256** — key derivation
* **AES-256-GCM** — authenticated encryption
* **SHA-256** — file integrity verification

The cryptographic and file-transfer logic is implemented in the platform-independent core.

## Build

### Core

```bash
mkdir build
cd build

cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
```

### Tests

```bash
./core/tests/airdrop_core_tests
```

### Go daemon

```bash
cd cmd/daemon

CGO_ENABLED=1 \
go build -o ../../build/daemon .

../../build/daemon
```

## Current Status

* [x] Binary protocol
* [x] Handshake
* [x] File offers
* [x] Chunk transfer
* [x] X25519 key exchange
* [x] AES-256-GCM encryption
* [x] SHA-256 integrity verification
* [x] Out-of-order chunk assembly
* [x] Loopback transport
* [x] C++ ↔ Go `cgo` bridge
* [ ] CoreBluetooth transport
* [ ] macOS menu-bar application
* [ ] iOS client
* [ ] Windows / C++ WinRT transport

## Roadmap

1. Complete the CoreBluetooth transport for macOS/iOS
2. Connect Bluetooth to the existing core
3. Build the macOS menu-bar application
4. Add iOS support
5. Implement Windows support using C++/WinRT
6. Improve transfer reliability and performance

## Why OffDrop?

OffDrop explores whether an AirDrop-like experience can be built using **Bluetooth as the only transport**, while keeping the protocol, cryptography, and file-transfer logic completely independent from the platform.

> **Status:** Experimental / Work in Progress
