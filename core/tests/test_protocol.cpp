#include "protocol.hpp"
#include "mini_test.hpp"

using namespace airdrop;

TEST(Hello_RoundTrip) {
    HelloPayload p;
    p.device_name = "Alexander's MacBook";
    p.x25519_public_key = std::vector<uint8_t>(32, 0xAB);

    auto encoded = EncodeHello(p);
    auto decoded = DecodeHello(encoded);

    ASSERT_EQ(decoded.device_name, p.device_name);
    ASSERT_TRUE(decoded.x25519_public_key == p.x25519_public_key);
}

TEST(FileOffer_RoundTrip) {
    FileOfferPayload p;
    p.file_name = "report.pdf";
    p.file_size = 123456789;
    p.sha256 = std::vector<uint8_t>(32, 0x11);
    p.total_chunks = 42;

    auto encoded = EncodeFileOffer(p);
    auto decoded = DecodeFileOffer(encoded);

    ASSERT_EQ(decoded.file_name, p.file_name);
    ASSERT_EQ(decoded.file_size, p.file_size);
    ASSERT_TRUE(decoded.sha256 == p.sha256);
    ASSERT_EQ(decoded.total_chunks, p.total_chunks);
}

TEST(Chunk_RoundTrip) {
    ChunkPayload p;
    p.chunk_index = 7;
    p.nonce = std::vector<uint8_t>(12, 0x22);
    p.ciphertext = std::vector<uint8_t>{1, 2, 3, 4, 5};
    p.tag = std::vector<uint8_t>(16, 0x33);

    auto encoded = EncodeChunk(p);
    auto decoded = DecodeChunk(encoded);

    ASSERT_EQ(decoded.chunk_index, p.chunk_index);
    ASSERT_TRUE(decoded.nonce == p.nonce);
    ASSERT_TRUE(decoded.ciphertext == p.ciphertext);
    ASSERT_TRUE(decoded.tag == p.tag);
}

TEST(FrameMessage_HeaderParsesCorrectly) {
    std::vector<uint8_t> payload = {9, 9, 9};
    auto framed = FrameMessage(MessageType::kFileOffer, payload);

    auto header = ParseHeader(framed);
    ASSERT_EQ(header.version, kProtocolVersion);
    ASSERT_TRUE(header.type == MessageType::kFileOffer);
    ASSERT_EQ(header.payload_len, (uint32_t)payload.size());
}

TEST(ParseHeader_RejectsUnknownVersion) {
    std::vector<uint8_t> bad = {99, 1, 0, 0, 0, 0};  // version=99
    ASSERT_THROWS(ParseHeader(bad));
}

TEST(ParseHeader_RejectsTruncatedData) {
    std::vector<uint8_t> tooShort = {1, 1};
    ASSERT_THROWS(ParseHeader(tooShort));
}

TEST(DecodeFileOffer_RejectsTruncatedPayload) {
    std::vector<uint8_t> garbage = {1, 2, 3};
    ASSERT_THROWS(DecodeFileOffer(garbage));
}
