// protocol.hpp
// Формат сообщений протокола передачи файлов без сети (Bluetooth-транспорт).
// Платформонезависимо: не зависит ни от CoreBluetooth, ни от WinRT, ни от чего-либо ОС-специфичного.
//
// Общая идея обмена:
//   1) HELLO      — узел A объявляет о себе (публичный ключ, имя устройства)
//   2) HELLO_ACK  — узел B отвечает своим публичным ключом
//      (после этого обе стороны считают общий секрет через X25519 -> AES-256-GCM ключ сессии)
//   3) FILE_OFFER — A предлагает файл (имя, размер, sha256)
//   4) FILE_ACCEPT / FILE_REJECT — B соглашается или отказывается
//   5) CHUNK*     — поток зашифрованных чанков файла с порядковым номером
//   6) FILE_DONE  — контрольная сумма подтверждена, передача завершена

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace airdrop {

// Версия протокола. Меняй при несовместимых изменениях формата.
constexpr uint8_t kProtocolVersion = 1;

// Размер чанка файла в байтах. 16 КБ — компромисс между накладными расходами
// на шифрование/заголовки и потерями при повторной передаче битого чанка
// по капризному Bluetooth-соединению.
constexpr uint32_t kChunkSize = 16 * 1024;

enum class MessageType : uint8_t {
    kHello = 0x01,
    kHelloAck = 0x02,
    kFileOffer = 0x03,
    kFileAccept = 0x04,
    kFileReject = 0x05,
    kChunk = 0x06,
    kFileDone = 0x07,
    kError = 0xFF,
};

// Заголовок, общий для всех сообщений на проводе.
// Wire layout (little-endian):
//   [0]      version   (1 байт)
//   [1]      type      (1 байт, MessageType)
//   [2..5]   payload_len (uint32, кол-во байт после заголовка)
//   [6..N]   payload
struct MessageHeader {
    uint8_t version = kProtocolVersion;
    MessageType type;
    uint32_t payload_len = 0;
};

constexpr size_t kHeaderSize = 1 + 1 + 4;

struct HelloPayload {
    std::string device_name;      // человекочитаемое имя, например "Alexander's MacBook"
    std::vector<uint8_t> x25519_public_key; // 32 байта
};

struct FileOfferPayload {
    std::string file_name;
    uint64_t file_size = 0;
    std::vector<uint8_t> sha256; // 32 байта, хэш незашифрованного файла
    uint32_t total_chunks = 0;
};

struct ChunkPayload {
    uint32_t chunk_index = 0;
    std::vector<uint8_t> nonce;       // 12 байт для AES-GCM, уникален на чанк
    std::vector<uint8_t> ciphertext;  // зашифрованные данные чанка
    std::vector<uint8_t> tag;         // 16-байтный auth tag GCM
};

// Сериализация / десериализация. Бросают std::runtime_error при повреждённых данных —
// на реальном Bluetooth-канале это ожидаемо (обрывы, битые пакеты), вызывающий код
// должен уметь просить повтор чанка, а не падать.
std::vector<uint8_t> EncodeHello(const HelloPayload& p);
HelloPayload DecodeHello(const std::vector<uint8_t>& payload);

std::vector<uint8_t> EncodeFileOffer(const FileOfferPayload& p);
FileOfferPayload DecodeFileOffer(const std::vector<uint8_t>& payload);

std::vector<uint8_t> EncodeChunk(const ChunkPayload& p);
ChunkPayload DecodeChunk(const std::vector<uint8_t>& payload);

// Собрать полное сообщение (заголовок + payload) для отправки в транспорт.
std::vector<uint8_t> FrameMessage(MessageType type, const std::vector<uint8_t>& payload);

// Распарсить заголовок из первых кБ, вернуть тип и указание сколько ещё байт payload'а ждать.
// Транспортный слой (Bluetooth) читает по кускам, поэтому парсинг header/payload разделены.
MessageHeader ParseHeader(const std::vector<uint8_t>& raw);

}  // namespace airdrop
