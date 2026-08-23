#include "crypto.hpp"

#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <fstream>
#include <memory>
#include <stdexcept>

namespace airdrop::crypto {

namespace {

// RAII-обёртка для EVP_PKEY_CTX, чтобы не забывать освобождать при исключениях.
using PkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using PkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using CipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

[[noreturn]] void ThrowOpenSSLError(const std::string& where) {
    throw std::runtime_error("crypto: OpenSSL error in " + where);
}

}  // namespace

KeyPair GenerateKeyPair() {
    PkeyCtxPtr pctx(EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr), EVP_PKEY_CTX_free);
    if (!pctx) ThrowOpenSSLError("EVP_PKEY_CTX_new_id");

    if (EVP_PKEY_keygen_init(pctx.get()) <= 0) ThrowOpenSSLError("keygen_init");

    EVP_PKEY* raw = nullptr;
    if (EVP_PKEY_keygen(pctx.get(), &raw) <= 0) ThrowOpenSSLError("keygen");
    PkeyPtr pkey(raw, EVP_PKEY_free);

    KeyPair kp;
    size_t pub_len = kX25519KeySize;
    size_t priv_len = kX25519KeySize;
    if (EVP_PKEY_get_raw_public_key(pkey.get(), kp.public_key.data(), &pub_len) <= 0)
        ThrowOpenSSLError("get_raw_public_key");
    if (EVP_PKEY_get_raw_private_key(pkey.get(), kp.private_key.data(), &priv_len) <= 0)
        ThrowOpenSSLError("get_raw_private_key");
    return kp;
}

KeyBytes DeriveSessionKey(const KeyBytes& my_private, const KeyBytes& peer_public,
                           const std::string& info) {
    PkeyPtr my_pkey(EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr, my_private.data(),
                                                   my_private.size()),
                     EVP_PKEY_free);
    if (!my_pkey) ThrowOpenSSLError("new_raw_private_key");

    PkeyPtr peer_pkey(EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, nullptr, peer_public.data(),
                                                    peer_public.size()),
                       EVP_PKEY_free);
    if (!peer_pkey) ThrowOpenSSLError("new_raw_public_key");

    PkeyCtxPtr dctx(EVP_PKEY_CTX_new(my_pkey.get(), nullptr), EVP_PKEY_CTX_free);
    if (!dctx) ThrowOpenSSLError("CTX_new(derive)");
    if (EVP_PKEY_derive_init(dctx.get()) <= 0) ThrowOpenSSLError("derive_init");
    if (EVP_PKEY_derive_set_peer(dctx.get(), peer_pkey.get()) <= 0)
        ThrowOpenSSLError("derive_set_peer");

    size_t secret_len = kX25519KeySize;
    std::array<uint8_t, kX25519KeySize> shared_secret{};
    if (EVP_PKEY_derive(dctx.get(), shared_secret.data(), &secret_len) <= 0)
        ThrowOpenSSLError("derive");

    // Прогоняем сырой DH-секрет через HKDF-SHA256, чтобы получить равномерно
    // распределённый ключ для AES, а не использовать сырую точку на кривой напрямую.
    KeyBytes session_key{};
    size_t out_len = kAesKeySize;

    PkeyCtxPtr hctx(EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr), EVP_PKEY_CTX_free);
    if (!hctx) ThrowOpenSSLError("CTX_new(hkdf)");
    if (EVP_PKEY_derive_init(hctx.get()) <= 0) ThrowOpenSSLError("hkdf derive_init");
    if (EVP_PKEY_CTX_set_hkdf_md(hctx.get(), EVP_sha256()) <= 0) ThrowOpenSSLError("hkdf set_md");
    // Пустая соль допустима по спецификации HKDF (RFC 5869), но некоторые версии OpenSSL
    // (например 3.6.x на свежем Homebrew) не принимают nullptr-указатель даже с длиной 0 —
    // передаём валидный указатель на пустой массив вместо nullptr для совместимости.
    static const uint8_t kEmptySalt[1] = {0};
    if (EVP_PKEY_CTX_set1_hkdf_salt(hctx.get(), kEmptySalt, 0) <= 0) ThrowOpenSSLError("hkdf salt");
    if (EVP_PKEY_CTX_set1_hkdf_key(hctx.get(), shared_secret.data(), (int)shared_secret.size()) <= 0)
        ThrowOpenSSLError("hkdf key");
    if (EVP_PKEY_CTX_add1_hkdf_info(hctx.get(), reinterpret_cast<const uint8_t*>(info.data()),
                                     (int)info.size()) <= 0)
        ThrowOpenSSLError("hkdf info");
    if (EVP_PKEY_derive(hctx.get(), session_key.data(), &out_len) <= 0)
        ThrowOpenSSLError("hkdf derive");

    return session_key;
}

EncryptedChunk EncryptChunk(const KeyBytes& session_key, const std::vector<uint8_t>& plaintext) {
    EncryptedChunk out;
    out.nonce.resize(kGcmNonceSize);
    if (RAND_bytes(out.nonce.data(), (int)out.nonce.size()) != 1) ThrowOpenSSLError("RAND_bytes nonce");

    CipherCtxPtr ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!ctx) ThrowOpenSSLError("CIPHER_CTX_new");

    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
        ThrowOpenSSLError("EncryptInit alg");
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, (int)kGcmNonceSize, nullptr) != 1)
        ThrowOpenSSLError("set ivlen");
    if (EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, session_key.data(), out.nonce.data()) != 1)
        ThrowOpenSSLError("EncryptInit key/iv");

    out.ciphertext.resize(plaintext.size());
    int out_len1 = 0;
    if (!plaintext.empty()) {
        if (EVP_EncryptUpdate(ctx.get(), out.ciphertext.data(), &out_len1, plaintext.data(),
                               (int)plaintext.size()) != 1)
            ThrowOpenSSLError("EncryptUpdate");
    }
    int out_len2 = 0;
    if (EVP_EncryptFinal_ex(ctx.get(), out.ciphertext.data() + out_len1, &out_len2) != 1)
        ThrowOpenSSLError("EncryptFinal");
    out.ciphertext.resize(out_len1 + out_len2);

    out.tag.resize(kGcmTagSize);
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, (int)kGcmTagSize, out.tag.data()) != 1)
        ThrowOpenSSLError("get tag");

    return out;
}

std::vector<uint8_t> DecryptChunk(const KeyBytes& session_key, const EncryptedChunk& chunk) {
    if (chunk.nonce.size() != kGcmNonceSize) throw std::runtime_error("crypto: bad nonce size");
    if (chunk.tag.size() != kGcmTagSize) throw std::runtime_error("crypto: bad tag size");

    CipherCtxPtr ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!ctx) ThrowOpenSSLError("CIPHER_CTX_new");

    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1)
        ThrowOpenSSLError("DecryptInit alg");
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, (int)kGcmNonceSize, nullptr) != 1)
        ThrowOpenSSLError("set ivlen");
    if (EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, session_key.data(), chunk.nonce.data()) != 1)
        ThrowOpenSSLError("DecryptInit key/iv");

    std::vector<uint8_t> plaintext(chunk.ciphertext.size());
    int out_len1 = 0;
    if (!chunk.ciphertext.empty()) {
        if (EVP_DecryptUpdate(ctx.get(), plaintext.data(), &out_len1, chunk.ciphertext.data(),
                               (int)chunk.ciphertext.size()) != 1)
            ThrowOpenSSLError("DecryptUpdate");
    }

    // Тег нужно передать до финализации — приводим const_cast, OpenSSL API here не const-safe.
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, (int)kGcmTagSize,
                             const_cast<uint8_t*>(chunk.tag.data())) != 1)
        ThrowOpenSSLError("set tag");

    int out_len2 = 0;
    int ok = EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + out_len1, &out_len2);
    if (ok <= 0) {
        // Это НЕ баг, а ожидаемый исход при повреждённых/поддельных данных — тег не совпал.
        throw std::runtime_error("crypto: chunk authentication failed (tampered or corrupted data)");
    }
    plaintext.resize(out_len1 + out_len2);
    return plaintext;
}

std::vector<uint8_t> Sha256Bytes(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> digest(SHA256_DIGEST_LENGTH);
    unsigned int out_len = 0;
    if (EVP_Digest(data.data(), data.size(), digest.data(), &out_len, EVP_sha256(), nullptr) != 1) {
        ThrowOpenSSLError("EVP_Digest");
    }
    return digest;
}

std::vector<uint8_t> Sha256File(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("crypto: cannot open file for hashing: " + path);

    // EVP_MD_CTX — современный (не deprecated в OpenSSL 3.0) интерфейс инкрементального хэширования.
    using MdCtxPtr = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
    MdCtxPtr ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!ctx) ThrowOpenSSLError("EVP_MD_CTX_new");
    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) ThrowOpenSSLError("DigestInit");

    std::vector<char> buf(1 << 16);
    while (f) {
        f.read(buf.data(), (std::streamsize)buf.size());
        std::streamsize n = f.gcount();
        if (n > 0) {
            if (EVP_DigestUpdate(ctx.get(), buf.data(), (size_t)n) != 1) ThrowOpenSSLError("DigestUpdate");
        }
    }
    std::vector<uint8_t> digest(SHA256_DIGEST_LENGTH);
    unsigned int out_len = 0;
    if (EVP_DigestFinal_ex(ctx.get(), digest.data(), &out_len) != 1) ThrowOpenSSLError("DigestFinal");
    return digest;
}

}  // namespace airdrop::crypto
