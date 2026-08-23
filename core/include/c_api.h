// c_api.h
// Плоский C ABI поверх core-библиотеки — единственная граница, через которую
// Go (через cgo) видит C++ core. Внутри cgo-обёртки в Go этот заголовок
// парсится cgo напрямую (см. cmd/daemon/bridge.go), поэтому здесь только
// простые типы: указатели, размеры, коды ошибок. Никаких C++ классов/шаблонов.
//
// Соглашение по памяти: любой буфер, который эта библиотека выделила
// (через malloc внутри), должен быть освобождён вызовом airdrop_free_buffer().

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef enum {
    AIRDROP_OK = 0,
    AIRDROP_ERR_INVALID_ARG = 1,
    AIRDROP_ERR_CRYPTO = 2,
    AIRDROP_ERR_IO = 3,
} airdrop_status;

typedef struct {
    uint8_t* data;
    size_t len;
} airdrop_buffer;

// Освобождает буфер, возвращённый любой из функций ниже.
void airdrop_free_buffer(airdrop_buffer buf);

// --- X25519 ---
// out_public/out_private должны указывать на буферы минимум по 32 байта.
airdrop_status airdrop_generate_keypair(uint8_t* out_public32, uint8_t* out_private32);

// info передаётся как обычная C-строка (например "airdrop-session-v1").
airdrop_status airdrop_derive_session_key(const uint8_t* my_private32, const uint8_t* peer_public32,
                                           const char* info, uint8_t* out_session_key32);

// --- AES-256-GCM ---
// session_key32 — 32 байта. Возвращает через out_* выделенные буферы
// (освободить через airdrop_free_buffer после использования).
airdrop_status airdrop_encrypt_chunk(const uint8_t* session_key32, const uint8_t* plaintext,
                                      size_t plaintext_len, airdrop_buffer* out_nonce,
                                      airdrop_buffer* out_ciphertext, airdrop_buffer* out_tag);

airdrop_status airdrop_decrypt_chunk(const uint8_t* session_key32, const uint8_t* nonce,
                                      size_t nonce_len, const uint8_t* ciphertext,
                                      size_t ciphertext_len, const uint8_t* tag, size_t tag_len,
                                      airdrop_buffer* out_plaintext);

// --- SHA-256 ---
// out_digest32 должен указывать на буфер минимум 32 байта.
airdrop_status airdrop_sha256_file(const char* path, uint8_t* out_digest32);

#ifdef __cplusplus
}  // extern "C"
#endif
