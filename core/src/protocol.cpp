#include "protocol.hpp"

#include <cstring>
#include <stdexcept>

namespace airdrop {

namespace {

void PutU32(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

uint32_t GetU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

void PutU64(std::vector<uint8_t>& buf, uint64_t v) {
    for (int i = 0; i < 8; ++i) buf.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
}

uint64_t GetU64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(p[i]) << (8 * i);
    return v;
}

void PutBytes(std::vector<uint8_t>& buf, const std::vector<uint8_t>& data) {
    PutU32(buf, static_cast<uint32_t>(data.size()));
    buf.insert(buf.end(), data.begin(), data.end());
}

void PutString(std::vector<uint8_t>& buf, const std::string& s) {
    PutU32(buf, static_cast<uint32_t>(s.size()));
    buf.insert(buf.end(), s.begin(), s.end());
}

// Читает length-prefixed блок из buf начиная с offset, продвигает offset.
std::vector<uint8_t> ReadBytes(const std::vector<uint8_t>& buf, size_t& offset) {
    if (offset + 4 > buf.size()) throw std::runtime_error("protocol: truncated length prefix");
    uint32_t len = GetU32(buf.data() + offset);
    offset += 4;
    if (offset + len > buf.size()) throw std::runtime_error("protocol: truncated payload");
    std::vector<uint8_t> out(buf.begin() + offset, buf.begin() + offset + len);
    offset += len;
    return out;
}

std::string ReadString(const std::vector<uint8_t>& buf, size_t& offset) {
    auto bytes = ReadBytes(buf, offset);
    return std::string(bytes.begin(), bytes.end());
}

}  // namespace

std::vector<uint8_t> FrameMessage(MessageType type, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> out;
    out.reserve(kHeaderSize + payload.size());
    out.push_back(kProtocolVersion);
    out.push_back(static_cast<uint8_t>(type));
    PutU32(out, static_cast<uint32_t>(payload.size()));
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

MessageHeader ParseHeader(const std::vector<uint8_t>& raw) {
    if (raw.size() < kHeaderSize) throw std::runtime_error("protocol: header too short");
    MessageHeader h;
    h.version = raw[0];
    h.type = static_cast<MessageType>(raw[1]);
    h.payload_len = GetU32(raw.data() + 2);
    if (h.version != kProtocolVersion) {
        throw std::runtime_error("protocol: version mismatch, peer uses a different protocol version");
    }
    return h;
}

std::vector<uint8_t> EncodeHello(const HelloPayload& p) {
    std::vector<uint8_t> buf;
    PutString(buf, p.device_name);
    PutBytes(buf, p.x25519_public_key);
    return buf;
}

HelloPayload DecodeHello(const std::vector<uint8_t>& payload) {
    size_t offset = 0;
    HelloPayload p;
    p.device_name = ReadString(payload, offset);
    p.x25519_public_key = ReadBytes(payload, offset);
    return p;
}

std::vector<uint8_t> EncodeFileOffer(const FileOfferPayload& p) {
    std::vector<uint8_t> buf;
    PutString(buf, p.file_name);
    PutU64(buf, p.file_size);
    PutBytes(buf, p.sha256);
    PutU32(buf, p.total_chunks);
    return buf;
}

FileOfferPayload DecodeFileOffer(const std::vector<uint8_t>& payload) {
    size_t offset = 0;
    FileOfferPayload p;
    p.file_name = ReadString(payload, offset);
    if (offset + 8 > payload.size()) throw std::runtime_error("protocol: truncated file_size");
    p.file_size = GetU64(payload.data() + offset);
    offset += 8;
    p.sha256 = ReadBytes(payload, offset);
    if (offset + 4 > payload.size()) throw std::runtime_error("protocol: truncated total_chunks");
    p.total_chunks = GetU32(payload.data() + offset);
    offset += 4;
    return p;
}

std::vector<uint8_t> EncodeChunk(const ChunkPayload& p) {
    std::vector<uint8_t> buf;
    PutU32(buf, p.chunk_index);
    PutBytes(buf, p.nonce);
    PutBytes(buf, p.ciphertext);
    PutBytes(buf, p.tag);
    return buf;
}

ChunkPayload DecodeChunk(const std::vector<uint8_t>& payload) {
    size_t offset = 0;
    ChunkPayload p;
    if (offset + 4 > payload.size()) throw std::runtime_error("protocol: truncated chunk_index");
    p.chunk_index = GetU32(payload.data() + offset);
    offset += 4;
    p.nonce = ReadBytes(payload, offset);
    p.ciphertext = ReadBytes(payload, offset);
    p.tag = ReadBytes(payload, offset);
    return p;
}

}  // namespace airdrop
