// bridge.go
// cgo-обёртка над core/include/c_api.h — единственное место в Go-коде,
// где встречается C. Дальше по коду (session.go, main.go) уже обычный
// идиоматичный Go, без cgo-специфики.
package main

/*
#cgo CXXFLAGS: -std=c++17
#cgo CFLAGS: -I${SRCDIR}/../../core/include
// Homebrew ставит OpenSSL не в системный путь (Apple намеренно не даёт system libcrypto
// для сборки, там только LibreSSL/CommonCrypto) — поэтому линковщику нужно явно указать,
// где искать -lcrypto. Указаны оба возможных префикса Homebrew: Apple Silicon (/opt/homebrew)
// и Intel Mac (/usr/local). Отсутствующий путь линковщик просто проигнорирует.
#cgo LDFLAGS: -L${SRCDIR}/../../build/core -L/opt/homebrew/opt/openssl@3/lib -L/usr/local/opt/openssl@3/lib -lairdrop_core_c -lairdrop_core -lcrypto -lstdc++

#include <stdlib.h>
#include "c_api.h"
*/
import "C"
import (
	"fmt"
	"unsafe"
)

// KeyPair — пара X25519 ключей на одну сессию (ephemeral, генерируется заново на каждую передачу).
type KeyPair struct {
	Public  [32]byte
	Private [32]byte
}

func GenerateKeyPair() (KeyPair, error) {
	var kp KeyPair
	status := C.airdrop_generate_keypair(
		(*C.uint8_t)(unsafe.Pointer(&kp.Public[0])),
		(*C.uint8_t)(unsafe.Pointer(&kp.Private[0])),
	)
	if status != C.AIRDROP_OK {
		return KeyPair{}, fmt.Errorf("airdrop_generate_keypair failed: status=%d", status)
	}
	return kp, nil
}

func DeriveSessionKey(myPrivate, peerPublic [32]byte, info string) ([32]byte, error) {
	var sessionKey [32]byte
	cInfo := C.CString(info)
	defer C.free(unsafe.Pointer(cInfo))

	status := C.airdrop_derive_session_key(
		(*C.uint8_t)(unsafe.Pointer(&myPrivate[0])),
		(*C.uint8_t)(unsafe.Pointer(&peerPublic[0])),
		cInfo,
		(*C.uint8_t)(unsafe.Pointer(&sessionKey[0])),
	)
	if status != C.AIRDROP_OK {
		return [32]byte{}, fmt.Errorf("airdrop_derive_session_key failed: status=%d", status)
	}
	return sessionKey, nil
}

// EncryptedChunk — результат шифрования одного чанка файла.
type EncryptedChunk struct {
	Nonce      []byte
	Ciphertext []byte
	Tag        []byte
}

func bufToBytes(buf C.airdrop_buffer) []byte {
	if buf.data == nil || buf.len == 0 {
		return nil
	}
	defer C.airdrop_free_buffer(buf)
	return C.GoBytes(unsafe.Pointer(buf.data), C.int(buf.len))
}

func EncryptChunk(sessionKey [32]byte, plaintext []byte) (EncryptedChunk, error) {
	var nonce, ciphertext, tag C.airdrop_buffer

	var ptPtr *C.uint8_t
	if len(plaintext) > 0 {
		ptPtr = (*C.uint8_t)(unsafe.Pointer(&plaintext[0]))
	}

	status := C.airdrop_encrypt_chunk(
		(*C.uint8_t)(unsafe.Pointer(&sessionKey[0])),
		ptPtr, C.size_t(len(plaintext)),
		&nonce, &ciphertext, &tag,
	)
	if status != C.AIRDROP_OK {
		return EncryptedChunk{}, fmt.Errorf("airdrop_encrypt_chunk failed: status=%d", status)
	}

	return EncryptedChunk{
		Nonce:      bufToBytes(nonce),
		Ciphertext: bufToBytes(ciphertext),
		Tag:        bufToBytes(tag),
	}, nil
}

func DecryptChunk(sessionKey [32]byte, chunk EncryptedChunk) ([]byte, error) {
	var plaintext C.airdrop_buffer

	status := C.airdrop_decrypt_chunk(
		(*C.uint8_t)(unsafe.Pointer(&sessionKey[0])),
		(*C.uint8_t)(unsafe.Pointer(&chunk.Nonce[0])), C.size_t(len(chunk.Nonce)),
		(*C.uint8_t)(unsafe.Pointer(&chunk.Ciphertext[0])), C.size_t(len(chunk.Ciphertext)),
		(*C.uint8_t)(unsafe.Pointer(&chunk.Tag[0])), C.size_t(len(chunk.Tag)),
		&plaintext,
	)
	if status != C.AIRDROP_OK {
		// В т.ч. попадаем сюда если GCM-тег не совпал — данные подделаны/повреждены на канале.
		return nil, fmt.Errorf("airdrop_decrypt_chunk failed (auth error or corrupted data): status=%d", status)
	}
	return bufToBytes(plaintext), nil
}

func Sha256File(path string) ([32]byte, error) {
	var digest [32]byte
	cPath := C.CString(path)
	defer C.free(unsafe.Pointer(cPath))

	status := C.airdrop_sha256_file(cPath, (*C.uint8_t)(unsafe.Pointer(&digest[0])))
	if status != C.AIRDROP_OK {
		return [32]byte{}, fmt.Errorf("airdrop_sha256_file failed: status=%d", status)
	}
	return digest, nil
}
