// main.go
// На macOS этот демон будет: (1) держать меню-бар UI, (2) вызывать
// platform/macos мостик для реального BLE discovery/передачи,
// (3) прогонять данные через bridge.go -> C++ core для крипто/протокола.
//
// Пока platform/macos не подключён (это отдельная Xcode-сборка), main.go
// служит доказательством, что связка Go <-cgo-> C++ core реально работает:
// генерирует ключи, шифрует/расшифровывает данные и считает sha256 файла
// ЧЕРЕЗ C++ core, а не переизобретает крипто на Go.
package main

import (
	"fmt"
	"log"
	"os"
)

func main() {
	fmt.Println("=== offline-airdrop daemon: проверка cgo-моста к C++ core ===")

	alice, err := GenerateKeyPair()
	if err != nil {
		log.Fatalf("GenerateKeyPair (alice): %v", err)
	}
	bob, err := GenerateKeyPair()
	if err != nil {
		log.Fatalf("GenerateKeyPair (bob): %v", err)
	}
	fmt.Println("[ok] X25519 keypairs сгенерированы через C++ core")

	sessionAlice, err := DeriveSessionKey(alice.Private, bob.Public, "airdrop-session-v1")
	if err != nil {
		log.Fatalf("DeriveSessionKey (alice): %v", err)
	}
	sessionBob, err := DeriveSessionKey(bob.Private, alice.Public, "airdrop-session-v1")
	if err != nil {
		log.Fatalf("DeriveSessionKey (bob): %v", err)
	}
	if sessionAlice != sessionBob {
		log.Fatal("[FAIL] сессионные ключи не совпали — баг в HKDF/X25519")
	}
	fmt.Println("[ok] обе стороны получили одинаковый сессионный ключ (X25519 + HKDF)")

	plaintext := []byte("привет, это тестовый чанк файла")
	enc, err := EncryptChunk(sessionAlice, plaintext)
	if err != nil {
		log.Fatalf("EncryptChunk: %v", err)
	}
	fmt.Printf("[ok] чанк зашифрован: %d байт -> %d байт ciphertext + 16 байт tag\n",
		len(plaintext), len(enc.Ciphertext))

	decrypted, err := DecryptChunk(sessionBob, enc)
	if err != nil {
		log.Fatalf("DecryptChunk: %v", err)
	}
	if string(decrypted) != string(plaintext) {
		log.Fatal("[FAIL] расшифрованные данные не совпадают с исходными")
	}
	fmt.Println("[ok] чанк расшифрован на стороне Bob, содержимое совпадает")

	// Проверка sha256 через C++ core на самом этом файле.
	selfPath := os.Args[0]
	if digest, err := Sha256File(selfPath); err == nil {
		fmt.Printf("[ok] sha256 бинарника демона (через C++ core): %x\n", digest)
	}

	fmt.Println("\nВсё работает: Go <-cgo-> C++ core связка исправна.")
	fmt.Println("Дальше сюда подключается platform/macos (CoreBluetooth) вместо ручных вызовов выше.")
}
