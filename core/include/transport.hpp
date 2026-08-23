// transport.hpp
// Абстракция транспорта. Core-логика (протокол, крипто, чанкинг) ничего не знает
// про Bluetooth — она работает через этот интерфейс.
//
// Платформенный слой реализует его:
//   - macOS/iOS: platform/macos/bluetooth_bridge.mm поверх CoreBluetooth
//   - Windows (позже): platform/windows поверх C++/WinRT Bluetooth API
//   - тесты: LoopbackTransport ниже — гоняет байты в памяти, без реального железа
//
// Это и есть та самая граница, из-за которой core можно шарить между платформами:
// протокол и крипто НИКОГДА не компилируются с ObjC++ или WinRT-заголовками.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace airdrop {

struct PeerInfo {
    std::string id;           // например MAC-адрес или CBPeripheral.identifier
    std::string display_name; // имя, объявленное в BLE advertisement
    int rssi = 0;             // мощность сигнала — можно сортировать "кто ближе"
};

// Интерфейс, который должен реализовать каждый платформенный мостик.
class ITransport {
public:
    virtual ~ITransport() = default;

    // Начать объявлять устройство как доступное для приёма (BLE advertising).
    virtual void StartAdvertising(const std::string& device_name) = 0;
    virtual void StopAdvertising() = 0;

    // Начать сканирование окружающих устройств.
    virtual void StartScanning(std::function<void(const PeerInfo&)> on_peer_found) = 0;
    virtual void StopScanning() = 0;

    // Установить соединение с пиром, найденным сканированием.
    virtual void Connect(const std::string& peer_id, std::function<void(bool success)> on_result) = 0;
    virtual void Disconnect(const std::string& peer_id) = 0;

    // Отправить сырые байты уже установленного соединения.
    // Core-слой сам разбивает сообщения на MTU-совместимые куски, если транспорту это нужно —
    // платформенная реализация просто гарантирует доставку целого блока по порядку (RFCOMM это даёт).
    virtual void Send(const std::string& peer_id, const std::vector<uint8_t>& data) = 0;

    // Колбэк на входящие данные. Вызывающий код (session.hpp) сам парсит protocol.hpp сообщения.
    virtual void SetOnDataReceived(
        std::function<void(const std::string& peer_id, const std::vector<uint8_t>& data)> cb) = 0;
};

// In-memory реализация для юнит-тестов и для отладки протокола без реального Bluetooth.
// Два LoopbackTransport можно соединить друг с другом через ConnectPeers() ниже.
class LoopbackTransport : public ITransport {
public:
    explicit LoopbackTransport(std::string self_id) : self_id_(std::move(self_id)) {}

    void StartAdvertising(const std::string&) override {}
    void StopAdvertising() override {}
    void StartScanning(std::function<void(const PeerInfo&)>) override {}
    void StopScanning() override {}
    void Connect(const std::string&, std::function<void(bool)> on_result) override {
        if (on_result) on_result(true);
    }
    void Disconnect(const std::string&) override {}

    void Send(const std::string& peer_id, const std::vector<uint8_t>& data) override {
        if (peer_ && peer_->on_data_) peer_->on_data_(self_id_, data);
    }

    void SetOnDataReceived(
        std::function<void(const std::string&, const std::vector<uint8_t>&)> cb) override {
        on_data_ = std::move(cb);
    }

    // Тестовая обвязка: связать два loopback-транспорта как если бы они видели друг друга.
    static void ConnectPeers(LoopbackTransport& a, LoopbackTransport& b) {
        a.peer_ = &b;
        b.peer_ = &a;
    }

private:
    std::string self_id_;
    LoopbackTransport* peer_ = nullptr;
    std::function<void(const std::string&, const std::vector<uint8_t>&)> on_data_;
};

}  // namespace airdrop
