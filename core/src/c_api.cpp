#include "c_api.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include "crypto.hpp"

namespace {

airdrop_buffer MakeBuffer(const std::vector<uint8_t>& src) {
    airdrop_buffer buf;
    buf.len = src.size();
    buf.data = static_cast<uint8_t*>(std::malloc(src.size()));
    if (buf.data && !src.empty()) {
        std::memcpy(buf.data, src.data(), src.size());
    }
    return buf;
}

}  // namespace

void airdrop_free_buffer(airdrop_buffer buf) {
    if (buf.data) std::free(buf.data);
}

airdrop_status airdrop_generate_keypair(uint8_t* out_public32, uint8_t* out_private32) {
    if (!out_public32 || !out_private32) return AIRDROP_ERR_INVALID_ARG;
    try {
        auto kp = airdrop::crypto::GenerateKeyPair();
        std::memcpy(out_public32, kp.public_key.data(), kp.public_key.size());
        std::memcpy(out_private32, kp.private_key.data(), kp.private_key.size());
        return AIRDROP_OK;
    } catch (...) {
        return AIRDROP_ERR_CRYPTO;
    }
}

airdrop_status airdrop_derive_session_key(const uint8_t* my_private32, const uint8_t* peer_public32,
                                           const char* info, uint8_t* out_session_key32) {
    if (!my_private32 || !peer_public32 || !info || !out_session_key32) return AIRDROP_ERR_INVALID_ARG;
    try {
        airdrop::crypto::KeyBytes priv{};
        airdrop::crypto::KeyBytes pub{};
        std::memcpy(priv.data(), my_private32, priv.size());
        std::memcpy(pub.data(), peer_public32, pub.size());
        auto session = airdrop::crypto::DeriveSessionKey(priv, pub, std::string(info));
        std::memcpy(out_session_key32, session.data(), session.size());
        return AIRDROP_OK;
    } catch (...) {
        return AIRDROP_ERR_CRYPTO;
    }
}

airdrop_status airdrop_encrypt_chunk(const uint8_t* session_key32, const uint8_t* plaintext,
                                      size_t plaintext_len, airdrop_buffer* out_nonce,
                                      airdrop_buffer* out_ciphertext, airdrop_buffer* out_tag) {
    if (!session_key32 || !out_nonce || !out_ciphertext || !out_tag) return AIRDROP_ERR_INVALID_ARG;
    try {
        airdrop::crypto::KeyBytes key{};
        std::memcpy(key.data(), session_key32, key.size());
        std::vector<uint8_t> pt(plaintext, plaintext + plaintext_len);
        auto enc = airdrop::crypto::EncryptChunk(key, pt);
        *out_nonce = MakeBuffer(enc.nonce);
        *out_ciphertext = MakeBuffer(enc.ciphertext);
        *out_tag = MakeBuffer(enc.tag);
        return AIRDROP_OK;
    } catch (...) {
        return AIRDROP_ERR_CRYPTO;
    }
}

airdrop_status airdrop_decrypt_chunk(const uint8_t* session_key32, const uint8_t* nonce,
                                      size_t nonce_len, const uint8_t* ciphertext,
                                      size_t ciphertext_len, const uint8_t* tag, size_t tag_len,
                                      airdrop_buffer* out_plaintext) {
    if (!session_key32 || !out_plaintext) return AIRDROP_ERR_INVALID_ARG;
    try {
        airdrop::crypto::KeyBytes key{};
        std::memcpy(key.data(), session_key32, key.size());
        airdrop::crypto::EncryptedChunk enc;
        enc.nonce.assign(nonce, nonce + nonce_len);
        enc.ciphertext.assign(ciphertext, ciphertext + ciphertext_len);
        enc.tag.assign(tag, tag + tag_len);
        auto plaintext = airdrop::crypto::DecryptChunk(key, enc);
        *out_plaintext = MakeBuffer(plaintext);
        return AIRDROP_OK;
    } catch (...) {
        // Сюда попадаем в т.ч. при провале аутентификации GCM-тега — см. crypto.cpp.
        return AIRDROP_ERR_CRYPTO;
    }
}

airdrop_status airdrop_sha256_file(const char* path, uint8_t* out_digest32) {
    if (!path || !out_digest32) return AIRDROP_ERR_INVALID_ARG;
    try {
        auto digest = airdrop::crypto::Sha256File(path);
        std::memcpy(out_digest32, digest.data(), digest.size());
        return AIRDROP_OK;
    } catch (...) {
        return AIRDROP_ERR_IO;
    }
}
