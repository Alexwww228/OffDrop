#include "crypto.hpp"
#include "mini_test.hpp"

using namespace airdrop::crypto;

TEST(KeyExchange_ProducesMatchingSessionKeyOnBothSides) {
    auto alice = GenerateKeyPair();
    auto bob = GenerateKeyPair();

    auto alice_session = DeriveSessionKey(alice.private_key, bob.public_key, "airdrop-session-v1");
    auto bob_session = DeriveSessionKey(bob.private_key, alice.public_key, "airdrop-session-v1");

    ASSERT_TRUE(alice_session == bob_session);
}

TEST(KeyExchange_DifferentInfoProducesDifferentKey) {
    auto alice = GenerateKeyPair();
    auto bob = GenerateKeyPair();

    auto k1 = DeriveSessionKey(alice.private_key, bob.public_key, "context-a");
    auto k2 = DeriveSessionKey(alice.private_key, bob.public_key, "context-b");

    ASSERT_TRUE(k1 != k2);
}

TEST(EncryptDecrypt_RoundTripRecoversPlaintext) {
    auto alice = GenerateKeyPair();
    auto bob = GenerateKeyPair();
    auto key = DeriveSessionKey(alice.private_key, bob.public_key, "airdrop-session-v1");

    std::vector<uint8_t> plaintext = {1, 2, 3, 4, 5, 255, 0, 42};
    auto enc = EncryptChunk(key, plaintext);
    auto decrypted = DecryptChunk(key, enc);

    ASSERT_EQ(decrypted, plaintext);
}

TEST(EncryptDecrypt_EmptyPlaintextWorks) {
    auto alice = GenerateKeyPair();
    auto key = DeriveSessionKey(alice.private_key, alice.public_key, "self-test");

    std::vector<uint8_t> empty;
    auto enc = EncryptChunk(key, empty);
    auto decrypted = DecryptChunk(key, enc);

    ASSERT_TRUE(decrypted.empty());
}

TEST(Decrypt_TamperedCiphertextFailsAuthentication) {
    auto alice = GenerateKeyPair();
    auto bob = GenerateKeyPair();
    auto key = DeriveSessionKey(alice.private_key, bob.public_key, "airdrop-session-v1");

    std::vector<uint8_t> plaintext = {10, 20, 30};
    auto enc = EncryptChunk(key, plaintext);
    enc.ciphertext[0] ^= 0xFF;  // симулируем повреждение/подделку на канале

    ASSERT_THROWS(DecryptChunk(key, enc));
}

TEST(Decrypt_WrongSessionKeyFailsAuthentication) {
    auto alice = GenerateKeyPair();
    auto bob = GenerateKeyPair();
    auto eve = GenerateKeyPair();

    auto real_key = DeriveSessionKey(alice.private_key, bob.public_key, "airdrop-session-v1");
    auto wrong_key = DeriveSessionKey(eve.private_key, bob.public_key, "airdrop-session-v1");

    auto enc = EncryptChunk(real_key, {1, 2, 3});
    ASSERT_THROWS(DecryptChunk(wrong_key, enc));
}

TEST(Sha256_KnownVector) {
    // sha256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    auto digest = Sha256Bytes({});
    ASSERT_EQ(digest.size(), (size_t)32);
    ASSERT_EQ(digest[0], 0xe3);
    ASSERT_EQ(digest[1], 0xb0);
    ASSERT_EQ(digest[31], 0x55);
}
