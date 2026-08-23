# platform/macos

## Quick Start: UI Application Without Xcode

Only **Apple Command Line Tools** are required. This is a lightweight package from Apple and does not include the full Xcode installation or require the App Store.

Install them if they are not already installed:

```bash
xcode-select --install
```

Check the installation with:

```bash
xcode-select -p
```

### Steps

#### 1. Build the core

If the core has not been built yet, see the root `README.md`:

```bash
cd offline-airdrop

mkdir -p build && cd build

cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)

make -j
```

#### 2. Build the application

Build the macOS application using the provided script.

No `.xcodeproj` is required — `swiftc` compiles the application directly into an `.app` bundle:

```bash
cd ../platform/macos/AirdropSelfTest

./build.sh
```

#### 3. Run the application

```bash
open AirdropSelfTest.app
```

The application opens a window containing:

* a C++ core self-test with 5 checks;
* key generation;
* X25519 key exchange;
* AES-GCM encryption;
* decryption;
* tamper detection;
* a real Bluetooth panel using CoreBluetooth.

The Bluetooth panel provides:

* **Start Advertising**
* **Start Scanning**

On the first launch, macOS will ask for Bluetooth permission. Allow access when prompted.

If `build.sh` fails, check the following common causes:

* the C++ core was not built in step 1;
* `openssl@3` is not installed through Homebrew;
* Apple Command Line Tools are not installed.

This is **not yet full Bluetooth file transfer**. The current implementation provides Bluetooth device discovery and validates the complete cryptographic flow. The actual transport implementation is the next step.

---

## Alternative: Building with Xcode

If Xcode is already installed, the application can also be built using Xcode.

Create a new project:

```text
File → New → Project → macOS → App
```

Then add the same source files manually and configure the Header Search Paths and Library Search Paths in the same way as `build.sh`.

However, using `build.sh` is simpler and does not require a full Xcode installation.

---

## Next Step: Full Bluetooth Transport

The transport implementation in this directory is **macOS-specific** because it uses `CoreBluetooth.framework`, which is not available in Linux or standard CI environments.

The platform-independent `core/` — including the protocol, cryptography, and chunking layers — is built and tested separately using CMake.

See the root `CMakeLists.txt` for the cross-platform core configuration.

---

## Current Implementation

### Bluetooth Bridge

`BluetoothBridge.h` / `BluetoothBridge.mm` contain the transport bridge skeleton with TODOs for the main `airdrop::ITransport` operations:

* device discovery;
* advertising;
* connection;
* sending data;
* receiving data.

### Bluetooth UUIDs

Generate your own UUIDs using `uuidgen` before implementing the transport and replace:

```text
kAirdropServiceUUID
```

and the corresponding characteristic UUIDs.

---

## How to Continue

Implement the transport in the following order.

### 1. Implement advertising

Implement:

```text
StartAdvertising
peripheralManagerDidUpdateState:
```

Use a second Mac with **LightBlue** to verify that the device is visible as a BLE peripheral with the expected service UUID.

---

### 2. Implement scanning

Implement:

```text
StartScanning
didDiscoverPeripheral:
```

Run two instances of the application on two Macs and verify that they can discover each other.

---

### 3. Implement the connection

Implement:

```text
Connect
didConnectPeripheral:
```

Then establish an L2CAP channel using:

```text
openL2CAPChannel:
```

and `CBL2CAPChannel`.

`CBL2CAPChannel` is available on macOS 10.13 and later.

L2CAP is preferred over manually splitting data across GATT characteristic writes because it provides a byte-stream-oriented transport better suited to the OffDrop protocol.

---

### 4. Connect the data streams

Implement:

```text
Send
DeliverIncomingData
```

Connect the `InputStream` / `OutputStream` provided by `CBL2CAPChannel` to the byte stream produced and consumed by `protocol.hpp`.

The resulting data flow should look like:

```text
┌───────────────┐
│   C++ Core    │
│               │
│   Protocol    │
└───────┬───────┘
        │
        │ bytes
        ▼
┌───────────────┐
│ ITransport    │
└───────┬───────┘
        │
        ▼
┌───────────────┐
│ Bluetooth     │
│ Bridge        │
└───────┬───────┘
        │
        ▼
   CoreBluetooth
        │
        ▼
   L2CAP Channel
```

---

### 5. Verify end-to-end transport

Once `Send` and `SetOnDataReceived` work between two Macs, the entire `core/` should already be usable without modification:

```text
protocol
   │
   ├── cryptography
   │
   ├── chunking
   │
   └── file transfer
          │
          ▼
      ITransport
          │
          ▼
   CoreBluetooth
```

This separation is the main reason the transport layer was isolated behind `ITransport`.

---

### 6. Build the menu-bar UI

For the native macOS application, the simplest approach is:

```text
NSStatusItem
      │
      ▼
NSPopover
      │
      ▼
SwiftUI content
```

This provides a native menu-bar application without having to implement the entire UI manually with AppKit.

---

## Testing Without Real Bluetooth

The Bluetooth transport does **not** need to be implemented before developing the protocol and cryptographic layers.

The complete protocol and cryptographic flow can already be tested using:

```text
core/tests/test_chunking_e2e.cpp
```

with:

```text
LoopbackTransport
```

This makes it possible to develop and debug:

* protocol serialization;
* encryption and decryption;
* chunking;
* out-of-order delivery;
* file reconstruction;
* integrity verification;

without Xcode, Bluetooth hardware, or a real BLE connection.

The architecture is therefore:

```text
                 ┌──────────────────┐
                 │   Protocol Core  │
                 └────────┬─────────┘
                          │
                     ITransport
                          │
             ┌────────────┴────────────┐
             │                         │
             ▼                         ▼
   LoopbackTransport           CoreBluetooth
       (testing)                 (macOS)
```

This allows most of the system to be developed and tested independently of the platform-specific Bluetooth implementation.
