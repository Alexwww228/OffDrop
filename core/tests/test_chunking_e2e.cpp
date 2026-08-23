// test_chunking_e2e.cpp
// Полная симуляция передачи файла между двумя "устройствами" через LoopbackTransport:
// handshake ключей -> file offer -> поток зашифрованных чанков -> проверка целостности.
// Ничего не знает про реальный Bluetooth — именно так эту логику можно тестировать
// на любой машине (в т.ч. в CI на Linux), не имея физического macOS/iOS устройства рядом.

#include <cstdio>
#include <fstream>

#include "chunking.hpp"
#include "crypto.hpp"
#include "mini_test.hpp"
#include "transport.hpp"

using namespace airdrop;

namespace {

std::string WriteTempFile(const std::string& name, size_t size_bytes) {
    std::string path = "/tmp/" + name;
    std::ofstream f(path, std::ios::binary);
    for (size_t i = 0; i < size_bytes; ++i) {
        f.put(static_cast<char>(i % 251));  // псевдослучайный, но детерминированный паттерн
    }
    return path;
}

}  // namespace

TEST(EndToEnd_SmallFileTransfersIntact) {
    // 1. Ключевой обмен (эмулируем HELLO/HELLO_ACK напрямую, без сериализации —
    //    сериализация HelloPayload уже покрыта test_protocol.cpp)
    auto alice_keys = crypto::GenerateKeyPair();
    auto bob_keys = crypto::GenerateKeyPair();
    auto session_key_alice =
        crypto::DeriveSessionKey(alice_keys.private_key, bob_keys.public_key, "airdrop-session-v1");
    auto session_key_bob =
        crypto::DeriveSessionKey(bob_keys.private_key, alice_keys.public_key, "airdrop-session-v1");
    ASSERT_TRUE(session_key_alice == session_key_bob);

    // 2. Готовим файл поменьше одного чанка, чтобы тест был быстрым
    std::string src_path = WriteTempFile("airdrop_test_small.bin", 500);
    auto offer = PrepareFileOffer(src_path);
    ASSERT_EQ(offer.total_chunks, (uint32_t)1);

    // 3. Транспорт: два loopback-конца, "соединённых" друг с другом
    LoopbackTransport alice_transport("alice");
    LoopbackTransport bob_transport("bob");
    LoopbackTransport::ConnectPeers(alice_transport, bob_transport);

    std::string dst_path = "/tmp/airdrop_test_small_received.bin";
    FileReceiver receiver(dst_path, offer.file_size, offer.total_chunks, offer.sha256);

    bob_transport.SetOnDataReceived([&](const std::string&, const std::vector<uint8_t>& raw) {
        auto header = ParseHeader(raw);
        ASSERT_TRUE(header.type == MessageType::kChunk);
        std::vector<uint8_t> payload(raw.begin() + kHeaderSize, raw.end());
        auto chunk = DecodeChunk(payload);
        receiver.ReceiveChunk(session_key_bob, chunk);
    });

    // 4. Alice режет файл на чанки, шифрует и шлёт через транспорт
    SendFileChunks(src_path, session_key_alice, [&](const ChunkPayload& chunk) {
        auto payload = EncodeChunk(chunk);
        auto framed = FrameMessage(MessageType::kChunk, payload);
        alice_transport.Send("bob", framed);
    });

    // 5. Проверяем, что на стороне Bob файл собрался и совпадает по хэшу
    ASSERT_TRUE(receiver.IsComplete());
    ASSERT_TRUE(receiver.VerifyIntegrity());

    std::remove(src_path.c_str());
    std::remove(dst_path.c_str());
}

TEST(EndToEnd_MultiChunkFileTransfersIntact) {
    auto alice_keys = crypto::GenerateKeyPair();
    auto bob_keys = crypto::GenerateKeyPair();
    auto key = crypto::DeriveSessionKey(alice_keys.private_key, bob_keys.public_key, "v1");

    // Три с половиной чанка, чтобы проверить и полные, и последний неполный чанк
    size_t size = static_cast<size_t>(kChunkSize) * 3 + 1234;
    std::string src_path = WriteTempFile("airdrop_test_multi.bin", size);
    auto offer = PrepareFileOffer(src_path);
    ASSERT_EQ(offer.total_chunks, (uint32_t)4);

    std::string dst_path = "/tmp/airdrop_test_multi_received.bin";
    FileReceiver receiver(dst_path, offer.file_size, offer.total_chunks, offer.sha256);

    SendFileChunks(src_path, key, [&](const ChunkPayload& chunk) {
        // Симулируем сеть напрямую, без транспортного слоя — фокус теста на chunking+crypto
        receiver.ReceiveChunk(key, chunk);
    });

    ASSERT_TRUE(receiver.IsComplete());
    ASSERT_TRUE(receiver.VerifyIntegrity());

    std::remove(src_path.c_str());
    std::remove(dst_path.c_str());
}

TEST(EndToEnd_OutOfOrderChunksStillAssembleCorrectly) {
    // На реальном Bluetooth-канале чанки могут прийти не по порядку при повторной отправке
    // после сбоя — проверяем, что сборка файла от этого не ломается.
    auto keys = crypto::GenerateKeyPair();
    auto key = crypto::DeriveSessionKey(keys.private_key, keys.public_key, "v1");

    size_t size = static_cast<size_t>(kChunkSize) * 2 + 500;
    std::string src_path = WriteTempFile("airdrop_test_reorder.bin", size);
    auto offer = PrepareFileOffer(src_path);

    std::vector<ChunkPayload> chunks;
    SendFileChunks(src_path, key, [&](const ChunkPayload& c) { chunks.push_back(c); });
    ASSERT_EQ(chunks.size(), (size_t)offer.total_chunks);

    // Разворачиваем порядок доставки
    std::reverse(chunks.begin(), chunks.end());

    std::string dst_path = "/tmp/airdrop_test_reorder_received.bin";
    FileReceiver receiver(dst_path, offer.file_size, offer.total_chunks, offer.sha256);
    for (auto& c : chunks) receiver.ReceiveChunk(key, c);

    ASSERT_TRUE(receiver.IsComplete());
    ASSERT_TRUE(receiver.VerifyIntegrity());

    std::remove(src_path.c_str());
    std::remove(dst_path.c_str());
}
