#!/bin/bash
# build.sh — собирает AirdropSelfTest.app БЕЗ Xcode, только через Command Line Tools
# (swiftc + системные фреймворки, которые есть в macOS SDK и без полного Xcode).
#
# Предварительно:
#   1) xcode-select --install   (если ещё не стоят Command Line Tools)
#   2) core уже собран через cmake в ../../build (см. корневой README.md) —
#      этот скрипт использует build/core/libairdrop_core_c.a и libairdrop_core.a
#   3) brew install openssl@3   (если ещё не стоит)

set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
CORE_INCLUDE="$ROOT/../../../core/include"
BUILD_CORE="$ROOT/../../../build/core"
APP_NAME="AirdropSelfTest"
APP_DIR="$ROOT/$APP_NAME.app"

if [ ! -f "$BUILD_CORE/libairdrop_core_c.a" ]; then
    echo "Не найден $BUILD_CORE/libairdrop_core_c.a"
    echo "Сначала собери core: cd ../../../ && mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j"
    exit 1
fi

OPENSSL_PREFIX="$(brew --prefix openssl@3 2>/dev/null || echo /opt/homebrew/opt/openssl@3)"
if [ ! -d "$OPENSSL_PREFIX" ]; then
    echo "Не найден OpenSSL по пути $OPENSSL_PREFIX — проверь 'brew install openssl@3'"
    exit 1
fi

echo "== Компилирую Swift-исходники в бинарник =="
rm -rf "$APP_DIR"
mkdir -p "$APP_DIR/Contents/MacOS"
mkdir -p "$APP_DIR/Contents/Resources"

ARCH="arm64"
DEPLOYMENT_TARGET="12.0"

swiftc \
    -target "${ARCH}-apple-macosx${DEPLOYMENT_TARGET}" \
    "$ROOT/AirdropSelfTestApp.swift" \
    "$ROOT/ContentView.swift" \
    "$ROOT/CoreSelfTest.swift" \
    "$ROOT/BluetoothManager.swift" \
    -import-objc-header "$ROOT/AirdropSelfTest-Bridging-Header.h" \
    -Xcc -I"$CORE_INCLUDE" \
    -I "$OPENSSL_PREFIX/include" \
    -L "$BUILD_CORE" -L "$OPENSSL_PREFIX/lib" \
    -lairdrop_core_c -lairdrop_core -lcrypto -lc++ \
    -framework SwiftUI -framework CoreBluetooth -framework AppKit -framework Foundation \
    -o "$APP_DIR/Contents/MacOS/$APP_NAME"

echo "== Пишу Info.plist =="
cat > "$APP_DIR/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>AirdropSelfTest</string>
    <key>CFBundleDisplayName</key><string>AirdropSelfTest</string>
    <key>CFBundleIdentifier</key><string>com.offlineairdrop.selftest</string>
    <key>CFBundleVersion</key><string>1.0</string>
    <key>CFBundleShortVersionString</key><string>1.0</string>
    <key>CFBundleExecutable</key><string>AirdropSelfTest</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>NSBluetoothAlwaysUsageDescription</key>
    <string>Приложению нужен доступ к Bluetooth для передачи файлов между устройствами без Wi-Fi.</string>
    <key>LSMinimumSystemVersion</key><string>12.0</string>
    <key>NSHighResolutionCapable</key><true/>
</dict>
</plist>
PLIST

echo "== Подписываю ad-hoc (чтобы macOS разрешил запуск локально) =="
codesign --force --deep --sign - "$APP_DIR"

echo ""
echo "Готово: $APP_DIR"
echo "Запусти: open \"$APP_DIR\""
