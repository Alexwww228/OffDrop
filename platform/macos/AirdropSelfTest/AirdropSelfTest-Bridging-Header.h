// AirdropSelfTest-Bridging-Header.h
// Objective-C Bridging Header — единственный способ дать Swift увидеть C API
// без прямого Swift/C++ interop (который требует более новых настроек Xcode).
// c_api.h написан на чистом C (extern "C"), поэтому Swift импортирует его отсюда
// как обычные C-функции — никакого Objective-C++ здесь не нужно.
//
// Xcode настройка: Build Settings -> Swift Compiler - General ->
// Objective-C Bridging Header -> путь к этому файлу.

#include "c_api.h"
