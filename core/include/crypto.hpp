// crypto.hpp
// X25519 обмен ключами для установления общего секрета между двумя устройствами
// (никакого сервера/интернета не требуется — чистый Diffie-Hellman по каналу Bluetooth),
// и AES-256-GCM для шифрования чанков файла с аутентификацией.
//
// Построено на OpenSSL (libcrypto), кроссплатформенно: одинаково линкуется
// на macOS, iOS и Windows.

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace airdrop::crypto {

constexpr size_t kX25519KeySize = 32;
constexpr size_t kAesKeySize = 32;   // AES-256
constexpr size_t kGcmNonceSize = 12; // рекомендованный размер nonce для GCM
constexpr size_t kGcmTagSize = 16;

using KeyBytes = std::array<uint8_t, kX25519KeySize>;

struct KeyPair {
    KeyBytes public_key{};
    KeyBytes private_key{};
};

// Сгенерировать новую пару ключей X25519 для этого сеанса.
// Новая пара должна генерироваться на КАЖДУЮ сессию (ephemeral keys) —
// это даёт forward secrecy: компрометация одной передачи не раскрывает прошлые.
KeyPair GenerateKeyPair();

// Вычислить общий секрет по своему приватному ключу и публичному ключу пира,
// затем прогнать через HKDF-SHA256, чтобы получить готовый 32-байтный AES-ключ.
// info используется как HKDF context — передавай что-то вроде "airdrop-session-v1".
KeyBytes DeriveSessionKey(const KeyBytes& my_private, const KeyBytes& peer_public,
                           const std::string& info);

struct EncryptedChunk {
    std::vector<uint8_t> nonce; // 12 байт, случайный на каждый чанк
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> tag;   // 16 байт
};

// Шифрует один чанк файла. Вызывающий код (chunking.cpp) отвечает за нарезку файла
// на чанки нужного размера — этот модуль знает только про крипто-операции.
EncryptedChunk EncryptChunk(const KeyBytes& session_key, const std::vector<uint8_t>& plaintext);

// Расшифровывает и проверяет подлинность чанка. Бросает std::runtime_error,
// если тег аутентификации не совпал (данные подделаны или повреждены на канале).
std::vector<uint8_t> DecryptChunk(const KeyBytes& session_key, const EncryptedChunk& chunk);

// SHA-256 всего файла — используется для FileOfferPayload.sha256 и финальной проверки целостности.
std::vector<uint8_t> Sha256File(const std::string& path);
std::vector<uint8_t> Sha256Bytes(const std::vector<uint8_t>& data);

}  // namespace airdrop::crypto
