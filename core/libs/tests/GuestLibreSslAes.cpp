#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, const char* what) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, what);
}

static std::vector<unsigned char> Bytes(const char* hex) {
    std::vector<unsigned char> bytes;
    for (; hex[0] && hex[1]; hex += 2) bytes.push_back(static_cast<unsigned char>(std::stoi(std::string(hex, 2), nullptr, 16)));
    return bytes;
}

static std::string Hex(const unsigned char* data, std::size_t size) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string text;
    for (std::size_t i = 0; i < size; ++i) {
        text += digits[data[i] >> 4];
        text += digits[data[i] & 15];
    }
    return text;
}

static std::uint32_t Word(const std::vector<unsigned char>& bytes, std::size_t index) {
    return std::uint32_t(bytes[4 * index]) << 24 | std::uint32_t(bytes[4 * index + 1]) << 16 | std::uint32_t(bytes[4 * index + 2]) << 8 | bytes[4 * index + 3];
}

static AesKey Sentinel() {
    AesKey key;
    for (std::uint32_t i = 0; i < 60; ++i) key.roundKeys[i] = 0xa5a50000u | i;
    key.rounds = 0x5a5a;
    return key;
}

template<typename TSetup> static void RequireSchedule(TSetup setup, const char* keyHex, const char* scheduleHex, const char* what) {
    const auto userKey = Bytes(keyHex);
    const auto schedule = Bytes(scheduleHex);
    const std::size_t used = schedule.size() / 4;
    const AesKey before = Sentinel();
    AesKey key = before;
    Require(setup(userKey.data(), static_cast<int>(userKey.size() * 8), &key) == 0 && key.rounds == static_cast<int>(used / 4 - 1), what);
    for (std::size_t i = 0; i < 60; ++i) Require(key.roundKeys[i] == (i < used ? Word(schedule, i) : before.roundKeys[i]), what);
}

template<typename TSetup> static void RequireRefusalsKeepKey(TSetup setup, const char* what) {
    const unsigned char userKey[32] {};
    const AesKey before = Sentinel();
    for (const int bits : {0, 64, 127, 129, 160, 224, 255, 384, 512, -128}) {
        AesKey key = before;
        Require(setup(userKey, bits, &key) == -2 && !std::memcmp(&key, &before, sizeof(key)), what);
    }
    for (const int bits : {128, 64}) {
        AesKey key = before;
        Require(setup(nullptr, bits, &key) == -1 && !std::memcmp(&key, &before, sizeof(key)), what);
    }
    Require(setup(userKey, 128, nullptr) == -1, what);
}

int main() {
    const auto plain = Bytes("00112233445566778899aabbccddeeff");
    const char* keys[3] {
        "000102030405060708090a0b0c0d0e0f",
        "000102030405060708090a0b0c0d0e0f1011121314151617",
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
    };
    const char* ciphers[3] {"69c4e0d86a7b0430d8cdb78070b4c55a", "dda97ca4864cdfe06eaf70a0ec0d7191", "8ea2b7ca516745bfeafc49904b496089"};
    for (int i = 0; i < 3; ++i) {
        const auto key = Bytes(keys[i]);
        const int bits = static_cast<int>(key.size() * 8);
        AesKey encrypt {}, decrypt {};
        unsigned char out[16] {}, back[16] {};
        Require(AES_set_encrypt_key_nid_postfix(key.data(), bits, &encrypt) == 0 && encrypt.rounds == 10 + 2 * i, "encrypt key");
        Require(AES_set_decrypt_key_nid_postfix(key.data(), bits, &decrypt) == 0 && decrypt.rounds == encrypt.rounds, "decrypt key");
        const int last = 4 * encrypt.rounds;
        Require(!std::memcmp(decrypt.roundKeys, encrypt.roundKeys + last, 16) && !std::memcmp(decrypt.roundKeys + last, encrypt.roundKeys, 16), "decrypt key reverses the round keys");
        AES_encrypt_nid_postfix(plain.data(), out, &encrypt);

        Require(Hex(out, 16) == ciphers[i], "FIPS-197 appendix C cipher");
        AES_decrypt_nid_postfix(out, back, &decrypt);

        Require(!std::memcmp(back, plain.data(), 16), "FIPS-197 appendix C inverse cipher");
        AES_ecb_encrypt_nid_postfix(plain.data(), out, &encrypt, 1);

        Require(Hex(out, 16) == ciphers[i], "ECB encrypt");
        AES_ecb_encrypt_nid_postfix(out, out, &decrypt, 0);

        Require(!std::memcmp(out, plain.data(), 16), "ECB decrypt in place");
    }

    AesKey fipsKey {};
    Require(AES_set_encrypt_key_nid_postfix(Bytes("2b7e151628aed2a6abf7158809cf4f3c").data(), 128, &fipsKey) == 0, "FIPS-197 appendix A key");
    Require(fipsKey.roundKeys[4] == 0xa0fafe17 && fipsKey.roundKeys[43] == 0xb6630ca6, "FIPS-197 appendix A.1 expansion");
    AesKey bad {};
    bad.rounds = 7;
    bad.roundKeys[0] = 1;
    const unsigned char zero[32] {};
    Require(AES_set_encrypt_key_nid_postfix(zero, 64, &bad) == -2 && bad.rounds == 7 && bad.roundKeys[0] == 1, "unsupported key length keeps the key");
    Require(AES_set_decrypt_key_nid_postfix(zero, 129, &bad) == -2 && bad.rounds == 7 && bad.roundKeys[0] == 1, "unsupported decrypt key length keeps the key");
    Require(AES_set_encrypt_key_nid_postfix(nullptr, 128, &bad) == -1 && AES_set_encrypt_key_nid_postfix(zero, 128, nullptr) == -1, "null key arguments");
    unsigned char block[16] {};
    RequireThrows([&] { AES_encrypt_nid_postfix(zero, block, &bad); }, "key schedule with an invalid round count");
    RequireRefusalsKeepKey(&AES_set_encrypt_key_nid_postfix, "refused encrypt key setup leaves the key unchanged");
    RequireRefusalsKeepKey(&AES_set_decrypt_key_nid_postfix, "refused decrypt key setup leaves the key unchanged");
    RequireSchedule(&AES_set_encrypt_key_nid_postfix, "2b7e151628aed2a6abf7158809cf4f3c",
        "2b7e151628aed2a6abf7158809cf4f3ca0fafe1788542cb123a339392a6c7605f2c295f27a96b9435935807a7359f67f3d80477d4716fe3e1e237e446d7a883b"
        "ef44a541a8525b7fb671253bdb0bad00d4d1c6f87c839d87caf2b8bc11f915bc6d88a37a110b3efddbf98641ca0093fd4e54f70e5f5fc9f384a64fb24ea6dc4f"
        "ead27321b58dbad2312bf5607f8d292fac7766f319fadc2128d12941575c006ed014f9a8c9ee2589e13f0cc8b6630ca6",
        "FIPS-197 appendix A.1 AES-128 encrypt schedule");
    RequireSchedule(&AES_set_encrypt_key_nid_postfix, "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
        "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7bfe0c91f72402f5a5ec12068e6c827f6b0e7a95b95c56fec24db7b4bd69b5411885a74796e92538fd"
        "e75fad44bb095386485af05721efb14fa448f6d94d6dce24aa326360113b30e6a25e7ed583b1cf9a27f939436a94f767c0a69407d19da4e1ec1786eb6fa64971"
        "485f703222cb8755e26d135233f0b7b340beeb282f18a2596747d26b458c553ea7e1466c9411f1df821f750aad07d753ca4005388fcc5006282d166abc3ce7b5"
        "e98ba06f448c773c8ecc720401002202",
        "FIPS-197 appendix A.2 AES-192 encrypt schedule");
    RequireSchedule(&AES_set_encrypt_key_nid_postfix, "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4",
        "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff49ba354118e6925afa51a8b5f2067fcdea8b09c1a93d194cdbe49846eb75d5b9a"
        "d59aecb85bf3c917fee94248de8ebe96b5a9328a2678a647983122292f6c79b3812c81addadf48ba24360af2fab8b46498c5bfc9bebd198e268c3ba709e04214"
        "68007bacb2df331696e939e46c518d80c814e20476a9fb8a5025c02d59c58239de1369676ccc5a71fa2563959674ee155886ca5d2e2f31d77e0af1fa27cf73c3"
        "749c47ab18501ddae2757e4f7401905acafaaae3e4d59b349adf6acebd10190dfe4890d1e6188d0b046df344706c631e",
        "FIPS-197 appendix A.3 AES-256 encrypt schedule");
    RequireSchedule(&AES_set_decrypt_key_nid_postfix, "2b7e151628aed2a6abf7158809cf4f3c",
        "d014f9a8c9ee2589e13f0cc8b6630ca60c7b5a631319eafeb0398890664cfbb4df7d925a1f62b09da320626ed675732412c07647c01f22c7bc42d2f37555114a"
        "6efcd876d2df54807c5df034c917c3b96ea30afcbc238cf6ae82a4b4b54a338d90884413d280860a12a128421bc897397c1f13f74208c219c021ae480969bf7b"
        "cc7505eb3e17d1ee82296c51c94811332b3708a7f262d405bc3ebdbf4b617d622b7e151628aed2a6abf7158809cf4f3c",
        "AES-128 decrypt schedule");
    RequireSchedule(&AES_set_decrypt_key_nid_postfix, "8e73b0f7da0e6452c810f32b809079e562f8ead2522c6b7b",
        "e98ba06f448c773c8ecc720401002202ac491644e55710b746c08a75c89b2cada3979ac28e5ba6d8e12cc9e654b272baf3b42258b59ebb5cf8fb64fe491e06f3"
        "4d65dfa2b1e5620dea899c312dcc3c1a5b6cfe3cc745a02bf8b9a572462a9904c5ddb7f8be933c760b4f46a6fc80bdafb5dc7ad0f7cffb09a7ec43939c295e17"
        "5023b89a3bc51d84d04b19377b4e8b8e41b34544ab0592b9ce92f15e421381d9659763e78c817087123039436be6a51e9eb149c479d69c5dfeb4a27ceab6d7fd"
        "8e73b0f7da0e6452c810f32b809079e5",
        "AES-192 decrypt schedule");
    RequireSchedule(&AES_set_decrypt_key_nid_postfix, "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4",
        "fe4890d1e6188d0b046df344706c631eada23f4963e23b2455427c8a5c70910457c96cf6074f07c0706abb07137f9241b668b621ce40046d36a047ae0932ed8e"
        "34ad1e4450866b367725bcc76315294632526c367828b24cf8e043c33f92aa20c440b289642b757227a3d7f114309581d669a7334a7ade7a80c8f18fc772e9e3"
        "25ba3c22a06bc7fb4388a2833393427054fb808b9c137949cab22ff547ba186c6c3d632985d1fbd9e3e36578701be0f34a7459f9c8e8f9c256a156bc8d083799"
        "42107758e9ec98f066329ea193f8858b8ec6bff6829ca03b9e49af7edba96125603deb1015ca71be2b73aef0857d7781",
        "AES-256 decrypt schedule");

    const auto key = Bytes("2b7e151628aed2a6abf7158809cf4f3c");
    const auto message = Bytes("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e5130c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710");
    AesKey encrypt {}, decrypt {};
    AES_set_encrypt_key_nid_postfix(key.data(), 128, &encrypt);
    AES_set_decrypt_key_nid_postfix(key.data(), 128, &decrypt);

    std::vector<unsigned char> out(message.size() + 16), back(message.size() + 16);

    auto iv = Bytes("000102030405060708090a0b0c0d0e0f");
    AES_cbc_encrypt_nid_postfix(message.data(), out.data(), 64, &encrypt, iv.data(), 1);

    Require(Hex(out.data(), 64) == "7649abac8119b246cee98e9b12e9197d5086cb9b507219ee95db113a917678b273bed6b8e3c1743b7116e69e222295163ff1caa1681fac09120eca307586e1a7", "SP 800-38A F.2.1 CBC encrypt");
    Require(Hex(iv.data(), 16) == "3ff1caa1681fac09120eca307586e1a7", "CBC encrypt returns the last cipher block as iv");
    iv = Bytes("000102030405060708090a0b0c0d0e0f");
    std::memcpy(back.data(), out.data(), 64);
    AES_cbc_encrypt_nid_postfix(back.data(), back.data(), 32, &decrypt, iv.data(), 0);
    AES_cbc_encrypt_nid_postfix(back.data() + 32, back.data() + 32, 32, &decrypt, iv.data(), 0);

    Require(!std::memcmp(back.data(), message.data(), 64) && Hex(iv.data(), 16) == "3ff1caa1681fac09120eca307586e1a7", "SP 800-38A F.2.2 CBC decrypt in place across calls");
    iv = Bytes("000102030405060708090a0b0c0d0e0f");
    std::vector<unsigned char> partial(32, 0xee);
    AES_cbc_encrypt_nid_postfix(message.data(), partial.data(), 20, &encrypt, iv.data(), 1);

    Require(Hex(partial.data(), 16) == "7649abac8119b246cee98e9b12e9197d" && partial[31] != 0xee && Hex(iv.data(), 16) == Hex(partial.data() + 16, 16), "CBC encrypt pads a partial block with the iv and writes the whole block");
    iv = Bytes("000102030405060708090a0b0c0d0e0f");
    std::vector<unsigned char> plainPartial(32, 0xee);
    AES_cbc_encrypt_nid_postfix(partial.data(), plainPartial.data(), 20, &decrypt, iv.data(), 0);

    Require(!std::memcmp(plainPartial.data(), message.data(), 20) && plainPartial[20] == 0xee && Hex(iv.data(), 16) == Hex(partial.data() + 16, 16), "CBC decrypt writes only the requested length");

    iv = Bytes("000102030405060708090a0b0c0d0e0f");
    int num = 0;
    AES_cfb128_encrypt_nid_postfix(message.data(), out.data(), 5, &encrypt, iv.data(), &num, 1);
    AES_cfb128_encrypt_nid_postfix(message.data() + 5, out.data() + 5, 32, &encrypt, iv.data(), &num, 1);

    Require(num == 5, "CFB128 keeps the block position");
    AES_cfb128_encrypt_nid_postfix(message.data() + 37, out.data() + 37, 27, &encrypt, iv.data(), &num, 1);

    Require(Hex(out.data(), 64) == "3b3fd92eb72dad20333449f8e83cfb4ac8a64537a0b3a93fcde3cdad9f1ce58b26751f67a3cbb140b1808cf187a4f4dfc04b05357c5d1c0eeac4c66f9ff7f2e6" && num == 0, "SP 800-38A F.3.13 CFB128 encrypt in pieces");
    iv = Bytes("000102030405060708090a0b0c0d0e0f");
    num = 0;
    AES_cfb128_encrypt_nid_postfix(out.data(), back.data(), 64, &encrypt, iv.data(), &num, 0);

    Require(!std::memcmp(back.data(), message.data(), 64), "SP 800-38A F.3.14 CFB128 decrypt");
    num = 16;
    RequireThrows([&] { AES_cfb128_encrypt_nid_postfix(message.data(), out.data(), 1, &encrypt, iv.data(), &num, 1); }, "CFB128 position out of range");

    auto counter = Bytes("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    unsigned char keystream[16] {};
    unsigned position = 0;
    AES_ctr128_encrypt_nid_postfix(message.data(), out.data(), 7, &encrypt, counter.data(), keystream, &position);
    AES_ctr128_encrypt_nid_postfix(message.data() + 7, out.data() + 7, 57, &encrypt, counter.data(), keystream, &position);

    Require(Hex(out.data(), 64) == "874d6191b620e3261bef6864990db6ce9806f66b7970fdff8617187bb9fffdff5ae4df3edbd5d35e5b4f09020db03eab1e031dda2fbe03d1792170a0f3009cee", "SP 800-38A F.5.1 CTR encrypt in pieces");
    Require(position == 0 && Hex(counter.data(), 16) == "f0f1f2f3f4f5f6f7f8f9fafbfcfdff03", "CTR advances the counter per block");
    counter = Bytes("ffffffffffffffffffffffffffffffff");
    position = 0;
    AES_ctr128_encrypt_nid_postfix(message.data(), out.data(), 37, &encrypt, counter.data(), keystream, &position);

    Require(Hex(out.data(), 37) == "e13338e36cb71962e00d020b4cedbd86d3dae15b04bb352fa0f59febfcb4da3e67da610697", "CTR counter wraps across all 128 bits");
    Require(position == 5 && Hex(counter.data(), 16) == "00000000000000000000000000000002", "CTR position and counter after a partial block");
    position = 16;
    RequireThrows([&] { AES_ctr128_encrypt_nid_postfix(message.data(), out.data(), 1, &encrypt, counter.data(), keystream, &position); }, "CTR position out of range");

    std::puts("libSceLibreSsl AES tests passed");
}
