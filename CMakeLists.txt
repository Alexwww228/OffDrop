cmake_minimum_required(VERSION 3.16)
project(offline_airdrop)

add_subdirectory(core)

# platform/macos не подключаем сюда: он требует Xcode toolchain (Objective-C++,
# CoreBluetooth.framework) и собирается отдельным Xcode-проектом/через xcodebuild
# на самой macOS. См. platform/macos/README.md.
