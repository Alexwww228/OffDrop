// BluetoothBridge.mm
// Скелет реализации. Компилируется только на macOS/iOS (Xcode, не CMake/Linux).
// Каждый TODO — это конкретный следующий шаг, а не "додумай сам с нуля":
// CoreBluetooth API, который туда нужен, уже назван.

#import "BluetoothBridge.h"

namespace {

// C++-обёртка вокруг ObjC-инстанса, реализующая airdrop::ITransport.
// Держит weak-подобную ссылку на BluetoothBridge* и просто форвардит вызовы.
class BluetoothTransportAdapter : public airdrop::ITransport {
public:
    explicit BluetoothTransportAdapter(BluetoothBridge* bridge) : bridge_(bridge) {}

    void StartAdvertising(const std::string& device_name) override {
        // TODO: bridge_.peripheralManager (CBPeripheralManager) должен уже быть poweredOn
        // (проверяется в peripheralManagerDidUpdateState:), затем:
        //   1) собрать CBMutableService с kAirdropServiceUUID и characteristic kAirdropRxCharUUID
        //   2) [peripheralManager addService:service]
        //   3) в peripheralManager:didAddService:error: вызвать startAdvertising с
        //      CBAdvertisementDataLocalNameKey = device_name и
        //      CBAdvertisementDataServiceUUIDsKey = @[kAirdropServiceUUID]
    }

    void StopAdvertising() override {
        // TODO: [bridge_.peripheralManager stopAdvertising]
    }

    void StartScanning(std::function<void(const airdrop::PeerInfo&)> on_peer_found) override {
        on_peer_found_ = std::move(on_peer_found);
        // TODO: [bridge_.centralManager scanForPeripheralsWithServices:@[[CBUUID UUIDWithString:kAirdropServiceUUID]]
        //                                                        options:nil]
        // Найденные пиры приходят в centralManager:didDiscoverPeripheral:advertisementData:RSSI:
        // (реализовано в BluetoothBridge ниже) — оттуда нужно дёрнуть on_peer_found_.
    }

    void StopScanning() override {
        // TODO: [bridge_.centralManager stopScan]
    }

    void Connect(const std::string& peer_id, std::function<void(bool)> on_result) override {
        pending_connect_callbacks_[peer_id] = std::move(on_result);
        // TODO: найти CBPeripheral по peer_id (сохранённому при discovery),
        // [bridge_.centralManager connectPeripheral:peripheral options:nil]
        // Результат приходит в centralManager:didConnectPeripheral: / didFailToConnectPeripheral:error:
    }

    void Disconnect(const std::string& peer_id) override {
        // TODO: [bridge_.centralManager cancelPeripheralConnection:peripheral]
    }

    void Send(const std::string& peer_id, const std::vector<uint8_t>& data) override {
        // TODO: два варианта транспорта данных на выбор при реализации:
        //   (а) BLE characteristic write — если данные маленькие, укладывать в MTU (~185-512 байт),
        //       резать на под-пакеты вручную поверх kChunkSize (двойная нарезка, но всё в рамках BLE);
        //   (б) поднять L2CAP-канал через CBPeripheral openL2CAPChannel: (доступно с iOS 11/macOS 10.13) —
        //       даёт потоковый сокет поверх Bluetooth, туда просто пишется целый framed-message
        //       из protocol.hpp без ручной нарезки. Это ближе к плану "BLE discovery + классический
        //       поток данных" — рекомендуется предпочесть L2CAP, а не GATT characteristic write.
    }

    void SetOnDataReceived(
        std::function<void(const std::string&, const std::vector<uint8_t>&)> cb) override {
        on_data_received_ = std::move(cb);
    }

    // Вызывается из BluetoothBridge при получении данных с L2CAP-канала или characteristic notify.
    void DeliverIncomingData(const std::string& peer_id, const std::vector<uint8_t>& data) {
        if (on_data_received_) on_data_received_(peer_id, data);
    }

    void NotifyPeerFound(const airdrop::PeerInfo& peer) {
        if (on_peer_found_) on_peer_found_(peer);
    }

    void NotifyConnectResult(const std::string& peer_id, bool success) {
        auto it = pending_connect_callbacks_.find(peer_id);
        if (it != pending_connect_callbacks_.end()) {
            it->second(success);
            pending_connect_callbacks_.erase(it);
        }
    }

private:
    __weak BluetoothBridge* bridge_;
    std::function<void(const airdrop::PeerInfo&)> on_peer_found_;
    std::function<void(const std::string&, const std::vector<uint8_t>&)> on_data_received_;
    std::unordered_map<std::string, std::function<void(bool)>> pending_connect_callbacks_;
};

}  // namespace

@implementation BluetoothBridge {
    CBCentralManager* _centralManager;
    CBPeripheralManager* _peripheralManager;
    std::shared_ptr<BluetoothTransportAdapter> _adapter;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _centralManager = [[CBCentralManager alloc] initWithDelegate:self queue:nil];
        _peripheralManager = [[CBPeripheralManager alloc] initWithDelegate:self queue:nil];
        _adapter = std::make_shared<BluetoothTransportAdapter>(self);
    }
    return self;
}

- (std::shared_ptr<airdrop::ITransport>)asTransport {
    return _adapter;
}

#pragma mark - CBCentralManagerDelegate

- (void)centralManagerDidUpdateState:(CBCentralManager*)central {
    // TODO: проверить central.state == CBManagerStatePoweredOn перед стартом сканирования;
    // если пользователь выключил Bluetooth — сообщить об этом в UI (менюбар-приложение).
}

- (void)centralManager:(CBCentralManager*)central
    didDiscoverPeripheral:(CBPeripheral*)peripheral
        advertisementData:(NSDictionary<NSString*, id>*)advertisementData
                     RSSI:(NSNumber*)RSSI {
    // TODO: собрать airdrop::PeerInfo { id = peripheral.identifier.UUIDString,
    //                                   display_name = advertisementData[CBAdvertisementDataLocalNameKey],
    //                                   rssi = RSSI.intValue }
    // и вызвать _adapter->NotifyPeerFound(peer). Не забыть сохранить peripheral в словаре
    // по его identifier, чтобы потом найти его в Connect().
}

- (void)centralManager:(CBCentralManager*)central didConnectPeripheral:(CBPeripheral*)peripheral {
    // TODO: peripheral.delegate = self; [peripheral discoverServices:@[[CBUUID UUIDWithString:kAirdropServiceUUID]]];
    // Финальный вызов _adapter->NotifyConnectResult(peer_id, true) — после того как сервис
    // и L2CAP-канал (или характеристики) реально открыты, а не сразу здесь.
}

#pragma mark - CBPeripheralManagerDelegate

- (void)peripheralManagerDidUpdateState:(CBPeripheralManager*)peripheral {
    // TODO: аналогично centralManagerDidUpdateState — проверка poweredOn перед advertising.
}

@end
