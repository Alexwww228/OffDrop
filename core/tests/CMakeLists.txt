add_executable(airdrop_core_tests
    test_crypto.cpp
    test_protocol.cpp
    test_chunking_e2e.cpp
    test_main.cpp
)
target_link_libraries(airdrop_core_tests PRIVATE airdrop_core)
add_test(NAME airdrop_core_tests COMMAND airdrop_core_tests)
