// BluetoothManager.swift
// Настоящая работа с CoreBluetooth напрямую из Swift (без Objective-C++ —
// у Swift есть нативный биндинг к CoreBluetooth, мостик из platform/macos/BluetoothBridge.mm
// нужен только когда логика будет жить в общем C++/Go core для шаринга между macOS/iOS;
// для этого self-test-приложения хватает чистого Swift).
//
// Что реально работает: advertising (устройство видно другим сканирующим),
// scanning (видим других). Передача файлов через L2CAP — следующий шаг,
// см. platform/macos/README.md.

import CoreBluetooth
import Foundation

let airdropServiceUUID = CBUUID(string: "E7810A71-73AE-499D-8C15-FAA9AAA5D172")

struct DiscoveredPeer: Identifiable, Equatable {
    let id: String
    let name: String
    let rssi: Int
}

final class BluetoothManager: NSObject, ObservableObject {
    @Published var isBluetoothPoweredOn = false
    @Published var isAdvertising = false
    @Published var isScanning = false
    @Published var discoveredPeers: [DiscoveredPeer] = []
    @Published var statusMessage = "Инициализация Bluetooth..."

    private var centralManager: CBCentralManager!
    private var peripheralManager: CBPeripheralManager!

    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: .main)
        peripheralManager = CBPeripheralManager(delegate: self, queue: .main)
    }

    func startAdvertising() {
        guard peripheralManager.state == .poweredOn else {
            statusMessage = "Bluetooth ещё не готов (peripheral manager не poweredOn)"
            return
        }
        let deviceName = Host.current().localizedName ?? "Mac"
        peripheralManager.startAdvertising([
            CBAdvertisementDataLocalNameKey: deviceName,
            CBAdvertisementDataServiceUUIDsKey: [airdropServiceUUID],
        ])
        isAdvertising = true
        statusMessage = "Объявляем устройство как «\(deviceName)» — видно другим сканирующим Mac рядом"
    }

    func stopAdvertising() {
        peripheralManager.stopAdvertising()
        isAdvertising = false
        statusMessage = "Объявление остановлено"
    }

    func startScanning() {
        guard centralManager.state == .poweredOn else {
            statusMessage = "Bluetooth ещё не готов (central manager не poweredOn)"
            return
        }
        discoveredPeers.removeAll()
        centralManager.scanForPeripherals(withServices: [airdropServiceUUID], options: nil)
        isScanning = true
        statusMessage = "Сканируем окружающие устройства с offline-airdrop..."
    }

    func stopScanning() {
        centralManager.stopScan()
        isScanning = false
        statusMessage = "Сканирование остановлено. Найдено пиров: \(discoveredPeers.count)"
    }
}

extension BluetoothManager: CBPeripheralManagerDelegate {
    func peripheralManagerDidUpdateState(_ peripheral: CBPeripheralManager) {
        switch peripheral.state {
        case .poweredOn:
            statusMessage = "Bluetooth включён и готов"
        case .poweredOff:
            statusMessage = "Bluetooth выключен — включи его в настройках macOS"
            isAdvertising = false
        case .unauthorized:
            statusMessage = "Нет разрешения на Bluetooth — проверь Настройки > Конфиденциальность"
        default:
            statusMessage = "Bluetooth недоступен: \(peripheral.state.rawValue)"
        }
    }
}

extension BluetoothManager: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        isBluetoothPoweredOn = central.state == .poweredOn
        if central.state == .poweredOn {
            statusMessage = "Bluetooth включён и готов"
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                         advertisementData: [String: Any], rssi RSSI: NSNumber) {
        let name = (advertisementData[CBAdvertisementDataLocalNameKey] as? String)
            ?? peripheral.name ?? "Неизвестное устройство"
        let peer = DiscoveredPeer(id: peripheral.identifier.uuidString, name: name, rssi: RSSI.intValue)
        if let index = discoveredPeers.firstIndex(where: { $0.id == peer.id }) {
            discoveredPeers[index] = peer
        } else {
            discoveredPeers.append(peer)
        }
    }
}
