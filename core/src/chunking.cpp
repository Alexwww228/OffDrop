#include "chunking.hpp"

#include <cmath>
#include <fstream>
#include <stdexcept>

namespace airdrop {

FileOfferPayload PrepareFileOffer(const std::string& file_path) {
    namespace fs = std::filesystem;
    if (!fs::exists(file_path)) throw std::runtime_error("chunking: file not found: " + file_path);

    FileOfferPayload offer;
    offer.file_name = fs::path(file_path).filename().string();
    offer.file_size = fs::file_size(file_path);
    offer.sha256 = crypto::Sha256File(file_path);
    offer.total_chunks =
        static_cast<uint32_t>((offer.file_size + kChunkSize - 1) / kChunkSize);
    if (offer.file_size == 0) offer.total_chunks = 0;
    return offer;
}

void SendFileChunks(const std::string& file_path, const crypto::KeyBytes& session_key,
                     const ChunkSink& sink) {
    std::ifstream f(file_path, std::ios::binary);
    if (!f) throw std::runtime_error("chunking: cannot open file: " + file_path);

    std::vector<uint8_t> buf(kChunkSize);
    uint32_t index = 0;
    while (f) {
        f.read(reinterpret_cast<char*>(buf.data()), (std::streamsize)buf.size());
        std::streamsize n = f.gcount();
        if (n <= 0) break;

        std::vector<uint8_t> plaintext(buf.begin(), buf.begin() + n);
        auto enc = crypto::EncryptChunk(session_key, plaintext);

        ChunkPayload payload;
        payload.chunk_index = index;
        payload.nonce = std::move(enc.nonce);
        payload.ciphertext = std::move(enc.ciphertext);
        payload.tag = std::move(enc.tag);

        sink(payload);
        ++index;
    }
}

FileReceiver::FileReceiver(std::string output_path, uint64_t expected_size, uint32_t total_chunks,
                             std::vector<uint8_t> expected_sha256)
    : output_path_(std::move(output_path)),
      expected_size_(expected_size),
      total_chunks_(total_chunks),
      expected_sha256_(std::move(expected_sha256)),
      received_mask_(total_chunks, false) {
    // Заранее выделяем файл нужного размера, чтобы писать чанки по смещению
    // в любом порядке (актуально, если решишь параллелить приём или переотправлять сбойные чанки).
    std::ofstream f(output_path_, std::ios::binary | std::ios::trunc);
    if (!f) throw std::runtime_error("chunking: cannot create output file: " + output_path_);
    if (expected_size_ > 0) {
        f.seekp(static_cast<std::streamoff>(expected_size_ - 1));
        f.put('\0');
    }
}

void FileReceiver::ReceiveChunk(const crypto::KeyBytes& session_key, const ChunkPayload& chunk) {
    if (chunk.chunk_index >= total_chunks_) {
        throw std::runtime_error("chunking: chunk_index out of range");
    }

    crypto::EncryptedChunk enc;
    enc.nonce = chunk.nonce;
    enc.ciphertext = chunk.ciphertext;
    enc.tag = chunk.tag;
    std::vector<uint8_t> plaintext = crypto::DecryptChunk(session_key, enc);

    std::fstream f(output_path_, std::ios::binary | std::ios::in | std::ios::out);
    if (!f) throw std::runtime_error("chunking: cannot open output file for writing");
    auto offset = static_cast<std::streamoff>(static_cast<uint64_t>(chunk.chunk_index) * kChunkSize);
    f.seekp(offset);
    f.write(reinterpret_cast<const char*>(plaintext.data()), (std::streamsize)plaintext.size());

    if (!received_mask_[chunk.chunk_index]) {
        received_mask_[chunk.chunk_index] = true;
        ++received_count_;
    }
}

bool FileReceiver::VerifyIntegrity() const {
    auto actual = crypto::Sha256File(output_path_);
    return actual == expected_sha256_;
}

}  // namespace airdrop
