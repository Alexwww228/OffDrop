// BluetoothBridge.h
// Objective-C++ мостик к CoreBluetooth, реализующий airdrop::ITransport (core/include/transport.hpp).
//
// ВАЖНО: этот файл собирается ТОЛЬКО на macOS/iOS через Xcode — здесь используется
// CoreBluetooth.framework, которого нет и не может быть в Linux-окружении.
// В этом репозитории (собираемом в Linux-контейнере) файл не компилируется —
// он написан "на будущее", когда разработка перейдёт на реальный Mac.
//
// Архитектурная роль: единственное место во всём проекте, где C++ код напрямую
// видит Objective-C API. core/ (протокол, крипто, chunking) ничего об этом не знает —
// он работает через транспортный интерфейс transport.hpp.

#pragma once

#import <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>

#include "transport.hpp"

// UUID сервиса и характеристик приложения. Сгенерируй свои через `uuidgen` перед стартом реализации —
// эти два должны быть одинаковыми на всех устройствах, чтобы они видели друг друга при сканировании.
static NSString* const kAirdropServiceUUID = @"E7810A71-73AE-499D-8C15-FAA9AAA5D172";
static NSString* const kAirdropRxCharUUID = @"BEF8D6C9-9C21-4C9E-B632-BD58C1009F9C";
static NSString* const kAirdropTxCharUUID = @"CA73B3BA-39F6-4A50-8B31-8F82E1CF9F9E";

// Пересылает объявленные события в C++ через std::function-колбэки из ITransport —
// сам класс написан на Objective-C++ (.mm), поэтому может свободно смешивать оба мира.
@interface BluetoothBridge : NSObject <CBCentralManagerDelegate, CBPeripheralManagerDelegate,
                                        CBPeripheralDelegate>

// Возвращает C++ объект, реализующий airdrop::ITransport и делегирующий вызовы
// в этот Objective-C класс. Используй его напрямую из Go/session-логики через transport.hpp.
- (std::shared_ptr<airdrop::ITransport>)asTransport;

@end
