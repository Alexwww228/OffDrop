// chunking.hpp
// Разбивает файл на зашифрованные чанки для отправки и собирает их обратно на приёме.
// Не знает ничего про Bluetooth — работает с абстрактным "отправь эти байты" через callback,
// поэтому один и тот же код используется и в тестах (mock transport), и в реальном приложении.

#pragma once

#include <filesystem>
#include <functional>
#include <string>

#include "crypto.hpp"
#include "protocol.hpp"

namespace airdrop {

// Вызывается для каждого зашифрованного чанка, готового к отправке в транспорт.
using ChunkSink = std::function<void(const ChunkPayload&)>;

// Разбивает файл на chunking-и, шифрует каждый сессионным ключом и передаёт в sink.
// Бросает std::runtime_error если файл не читается.
// Возвращает FileOfferPayload, который нужно отправить ДО начала чанков (описывает файл получателю).
FileOfferPayload PrepareFileOffer(const std::string& file_path);
void SendFileChunks(const std::string& file_path, const crypto::KeyBytes& session_key,
                     const ChunkSink& sink);

// Принимающая сторона: собирает чанки по мере получения (могут приходить не по порядку
// из-за повторных передач при ошибках канала) и пишет расшифрованные данные в выходной файл.
// Используй один экземпляр на одну входящую передачу.
class FileReceiver {
public:
    FileReceiver(std::string output_path, uint64_t expected_size, uint32_t total_chunks,
                 std::vector<uint8_t> expected_sha256);

    // Расшифровывает и записывает чанк по правильному смещению (chunk_index * kChunkSize).
    // Бросает std::runtime_error при ошибке аутентификации (см. crypto::DecryptChunk).
    void ReceiveChunk(const crypto::KeyBytes& session_key, const ChunkPayload& chunk);

    bool IsComplete() const { return received_count_ == total_chunks_; }

    // Вызывать после IsComplete() == true. Проверяет sha256 всего файла.
    // Возвращает true если хэш совпал (файл цел), false если повреждён.
    bool VerifyIntegrity() const;

private:
    std::string output_path_;
    uint64_t expected_size_;
    uint32_t total_chunks_;
    uint32_t received_count_ = 0;
    std::vector<uint8_t> expected_sha256_;
    std::vector<bool> received_mask_;
};

}  // namespace airdrop
