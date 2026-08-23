cmake_minimum_required(VERSION 3.16)
project(airdrop_core CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(OpenSSL REQUIRED)

add_library(airdrop_core STATIC
    src/protocol.cpp
    src/crypto.cpp
    src/chunking.cpp
)

target_include_directories(airdrop_core PUBLIC include)
target_link_libraries(airdrop_core PUBLIC OpenSSL::Crypto)

# extern "C" обёртка, через которую Go (cgo) будет дёргать этот core.
add_library(airdrop_core_c STATIC
    src/c_api.cpp
)
target_include_directories(airdrop_core_c PUBLIC include)
target_link_libraries(airdrop_core_c PUBLIC airdrop_core)

enable_testing()
add_subdirectory(tests)
