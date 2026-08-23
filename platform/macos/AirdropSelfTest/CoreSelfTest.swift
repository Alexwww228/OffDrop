// CoreSelfTest.swift
// Прогоняет ту же самую проверку, что я запускал в терминале через Go-демон,
// но теперь напрямую из Swift через c_api.h (см. bridging header) — без Go вообще.
// Каждый шаг — реальный вызов в C++ core (core/src/crypto.cpp), не заглушка.

import Foundation

struct SelfTestStep: Identifiable {
    let id = UUID()
    let title: String
    let success: Bool
    let detail: String
}

enum CoreSelfTest {
    static func run() -> [SelfTestStep] {
        var steps: [SelfTestStep] = []

        // 1. Генерация двух пар ключей X25519 через C++ core.
        var alicePub = [UInt8](repeating: 0, count: 32)
        var alicePriv = [UInt8](repeating: 0, count: 32)
        var bobPub = [UInt8](repeating: 0, count: 32)
        var bobPriv = [UInt8](repeating: 0, count: 32)

        let keygenAliceStatus = airdrop_generate_keypair(&alicePub, &alicePriv)
        let keygenBobStatus = airdrop_generate_keypair(&bobPub, &bobPriv)
        let keygenOk = keygenAliceStatus == AIRDROP_OK && keygenBobStatus == AIRDROP_OK
        steps.append(SelfTestStep(
            title: "Генерация X25519 ключей",
            success: keygenOk,
            detail: keygenOk ? "Alice и Bob получили пары ключей" : "airdrop_generate_keypair вернул ошибку"
        ))
        guard keygenOk else { return steps }

        // 2. Оба выводят общий сессионный ключ (Diffie-Hellman) и должны получить одно и то же.
        var sessionAlice = [UInt8](repeating: 0, count: 32)
        var sessionBob = [UInt8](repeating: 0, count: 32)
        let info = "airdrop-session-v1"

        let deriveAliceStatus = info.withCString { infoPtr in
            airdrop_derive_session_key(&alicePriv, &bobPub, infoPtr, &sessionAlice)
        }
        let deriveBobStatus = info.withCString { infoPtr in
            airdrop_derive_session_key(&bobPriv, &alicePub, infoPtr, &sessionBob)
        }
        let deriveOk = deriveAliceStatus == AIRDROP_OK && deriveBobStatus == AIRDROP_OK
            && sessionAlice == sessionBob
        steps.append(SelfTestStep(
            title: "Обмен ключами (X25519 + HKDF)",
            success: deriveOk,
            detail: deriveOk
                ? "Оба устройства независимо вычислили одинаковый сессионный ключ"
                : "Сессионные ключи не совпали или произошла ошибка"
        ))
        guard deriveOk else { return steps }

        // 3. Шифруем чанк на стороне Alice, расшифровываем на стороне Bob сессионным ключом Bob.
        let plaintext = Array("привет, это тестовый чанк файла".utf8)
        var nonceBuf = airdrop_buffer()
        var ciphertextBuf = airdrop_buffer()
        var tagBuf = airdrop_buffer()

        let encryptStatus = plaintext.withUnsafeBufferPointer { ptBuf in
            airdrop_encrypt_chunk(&sessionAlice, ptBuf.baseAddress, ptBuf.count,
                                   &nonceBuf, &ciphertextBuf, &tagBuf)
        }
        let encryptOk = encryptStatus == AIRDROP_OK
        steps.append(SelfTestStep(
            title: "Шифрование чанка (AES-256-GCM)",
            success: encryptOk,
            detail: encryptOk
                ? "\(plaintext.count) байт -> \(ciphertextBuf.len) байт ciphertext + tag"
                : "airdrop_encrypt_chunk вернул ошибку"
        ))
        guard encryptOk else { return steps }

        let nonce = Array(UnsafeBufferPointer(start: nonceBuf.data, count: nonceBuf.len))
        let ciphertext = Array(UnsafeBufferPointer(start: ciphertextBuf.data, count: ciphertextBuf.len))
        let tag = Array(UnsafeBufferPointer(start: tagBuf.data, count: tagBuf.len))
        airdrop_free_buffer(nonceBuf)
        airdrop_free_buffer(ciphertextBuf)
        airdrop_free_buffer(tagBuf)

        var decryptedBuf = airdrop_buffer()
        let decryptStatus = nonce.withUnsafeBufferPointer { noncePtr in
            ciphertext.withUnsafeBufferPointer { ctPtr in
                tag.withUnsafeBufferPointer { tagPtr in
                    airdrop_decrypt_chunk(&sessionBob, noncePtr.baseAddress, noncePtr.count,
                                           ctPtr.baseAddress, ctPtr.count,
                                           tagPtr.baseAddress, tagPtr.count, &decryptedBuf)
                }
            }
        }
        let decryptOk = decryptStatus == AIRDROP_OK
        var decryptedText = ""
        if decryptOk {
            let decrypted = Array(UnsafeBufferPointer(start: decryptedBuf.data, count: decryptedBuf.len))
            airdrop_free_buffer(decryptedBuf)
            decryptedText = String(bytes: decrypted, encoding: .utf8) ?? "<не utf8>"
        }
        let roundTripOk = decryptOk && decryptedText == String(bytes: plaintext, encoding: .utf8)
        steps.append(SelfTestStep(
            title: "Расшифровка на стороне Bob",
            success: roundTripOk,
            detail: roundTripOk
                ? "Получено: «\(decryptedText)» — совпадает с оригиналом"
                : "Расшифровка провалилась или текст не совпал"
        ))

        // 4. Портит тег и проверяет, что подделка действительно детектируется (это важно для безопасности).
        var tamperedTag = tag
        if !tamperedTag.isEmpty { tamperedTag[0] ^= 0xFF }
        var tamperedOutBuf = airdrop_buffer()
        let tamperedStatus = nonce.withUnsafeBufferPointer { noncePtr in
            ciphertext.withUnsafeBufferPointer { ctPtr in
                tamperedTag.withUnsafeBufferPointer { tagPtr in
                    airdrop_decrypt_chunk(&sessionBob, noncePtr.baseAddress, noncePtr.count,
                                           ctPtr.baseAddress, ctPtr.count,
                                           tagPtr.baseAddress, tagPtr.count, &tamperedOutBuf)
                }
            }
        }
        let tamperDetected = tamperedStatus != AIRDROP_OK
        if tamperedStatus == AIRDROP_OK { airdrop_free_buffer(tamperedOutBuf) }
        steps.append(SelfTestStep(
            title: "Детектирование подделки данных",
            success: tamperDetected,
            detail: tamperDetected
                ? "Повреждённый тег корректно отклонён (auth tag не совпал)"
                : "ОПАСНО: подделанные данные были приняты как валидные"
        ))

        return steps
    }
}
