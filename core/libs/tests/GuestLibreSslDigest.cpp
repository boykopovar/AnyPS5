#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
int APS5_VABI MD4_Init_nid_postfix(void*);
int APS5_VABI MD4_Update_nid_postfix(void*, const void*, std::size_t);
int APS5_VABI MD4_Final_nid_postfix(unsigned char*, void*);
int APS5_VABI MD5_Init_nid_postfix(void*);
int APS5_VABI MD5_Update_nid_postfix(void*, const void*, std::size_t);
int APS5_VABI MD5_Final_nid_postfix(unsigned char*, void*);
int APS5_VABI SHA1_Init_nid_postfix(void*);
int APS5_VABI SHA1_Update_nid_postfix(void*, const void*, std::size_t);
int APS5_VABI SHA1_Final_nid_postfix(unsigned char*, void*);
int APS5_VABI SHA224_Init_nid_postfix(void*);
int APS5_VABI SHA224_Update_nid_postfix(void*, const void*, std::size_t);
int APS5_VABI SHA224_Final_nid_postfix(unsigned char*, void*);
int APS5_VABI SHA256_Init_nid_postfix(void*);
int APS5_VABI SHA256_Update_nid_postfix(void*, const void*, std::size_t);
int APS5_VABI SHA256_Final_nid_postfix(unsigned char*, void*);
unsigned char* APS5_VABI SHA256_nid_postfix(const unsigned char*, std::size_t, unsigned char*);
int APS5_VABI SHA384_Init_nid_postfix(void*);
int APS5_VABI SHA384_Update_nid_postfix(void*, const void*, std::size_t);
int APS5_VABI SHA384_Final_nid_postfix(unsigned char*, void*);
int APS5_VABI SHA512_Init_nid_postfix(void*);
int APS5_VABI SHA512_Update_nid_postfix(void*, const void*, std::size_t);
int APS5_VABI SHA512_Final_nid_postfix(unsigned char*, void*);
}

using InitFunction = int (APS5_VABI*)(void*);
using UpdateFunction = int (APS5_VABI*)(void*, const void*, std::size_t);
using FinalFunction = int (APS5_VABI*)(unsigned char*, void*);

struct Algorithm {
    const char* name;
    InitFunction init;
    UpdateFunction update;
    FinalFunction final;
    std::size_t contextSize;
    std::size_t blockSize;
    std::size_t digestSize;
    const char* expected[6];
};

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, const char* what) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, what);
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

static std::string Digest(const Algorithm& algorithm, const std::string& message, std::size_t chunk) {
    alignas(16) unsigned char context[256] {};
    unsigned char digest[64] {};
    Require(algorithm.init(context) == 1, "init returns 1");
    for (std::size_t offset = 0; offset < message.size(); offset += chunk) {
        const auto length = std::min(chunk, message.size() - offset);
        Require(algorithm.update(context, message.data() + offset, length) == 1, "update returns 1");
    }
    Require(algorithm.final(digest, context) == 1, "final returns 1");
    return Hex(digest, algorithm.digestSize);
}

int main() {
    const std::string messages[6] {
        "",
        "abc",
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
        "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
        "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
        std::string(1000000, 'a'),
    };
    const Algorithm algorithms[] {
        {"MD4", MD4_Init_nid_postfix, MD4_Update_nid_postfix, MD4_Final_nid_postfix, 92, 64, 16, {
            "31d6cfe0d16ae931b73c59d7e0c089c0", "a448017aaf21d8525fc10ae87aa6729d", "4691a9ec81b1a6bd1ab8557240b245c5",
            nullptr, "e33b4ddc9c38f2199c3e7b164fcc0536", nullptr}},
        {"MD5", MD5_Init_nid_postfix, MD5_Update_nid_postfix, MD5_Final_nid_postfix, 92, 64, 16, {
            "d41d8cd98f00b204e9800998ecf8427e", "900150983cd24fb0d6963f7d28e17f72", "8215ef0796a20bcaaae116d3876c664a",
            "03dd8807a93175fb062dfb55dc7d359c", "57edf4a22be3c955ac49da2e2107b67a", "7707d6ae4e027c70eea2a935c2296f21"}},
        {"SHA1", SHA1_Init_nid_postfix, SHA1_Update_nid_postfix, SHA1_Final_nid_postfix, 96, 64, 20, {
            "da39a3ee5e6b4b0d3255bfef95601890afd80709", "a9993e364706816aba3e25717850c26c9cd0d89d", "84983e441c3bd26ebaae4aa1f95129e5e54670f1",
            "a49b2446a02c645bf419f995b67091253a04a259", "50abf5706a150990a08b2c5ea40fa0e585554732", "34aa973cd4c4daa4f61eeb2bdbad27316534016f"}},
        {"SHA224", SHA224_Init_nid_postfix, SHA224_Update_nid_postfix, SHA224_Final_nid_postfix, 112, 64, 28, {
            "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f", "23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7",
            "75388b16512776cc5dba5da1fd890150b0c6455cb4f58b1952522525", "c97ca9a559850ce97a04a96def6d99a9e0e0e2ab14e6b8df265fc0b3",
            "b50aecbe4e9bb0b57bc5f3ae760a8e01db24f203fb3cdcd13148046e", "20794655980c91d8bbb4c1ea97618a4bf03f42581948b2ee4ee7ad67"}},
        {"SHA256", SHA256_Init_nid_postfix, SHA256_Update_nid_postfix, SHA256_Final_nid_postfix, 112, 64, 32, {
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1",
            "f371bc4a311f2b009eef952dd83ca80e2b60026c8e935592d0f9c308453c813e", "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"}},
        {"SHA384", SHA384_Init_nid_postfix, SHA384_Update_nid_postfix, SHA384_Final_nid_postfix, 216, 128, 48, {
            "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b",
            "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7",
            "3391fdddfc8dc7393707a65b1b4709397cf8b1d162af05abfe8f450de5f36bc6b0455a8520bc4e6f5fe95b1fe3c8452b",
            "09330c33f71147e83d192fc782cd1b4753111b173b3b05d22fa08086e3b0f712fcc7c71a557e2db966c3e9fa91746039",
            "b12932b0627d1c060942f5447764155655bd4da0c9afa6dd9b9ef53129af1b8fb0195996d2de9ca0df9d821ffee67026",
            "9d0e1809716474cb086e834e310a4a1ced149e9c00f248527972cec5704c2a5b07b8b3dc38ecc4ebae97ddd87f3d8985"}},
        {"SHA512", SHA512_Init_nid_postfix, SHA512_Update_nid_postfix, SHA512_Final_nid_postfix, 216, 128, 64, {
            "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e",
            "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f",
            "204a8fc6dda82f0a0ced7beb8e08a41657c16ef468b228a8279be331a703c33596fd15c13b1b07f9aa1d3bea57789ca031ad85c7a71dd70354ec631238ca3445",
            "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909",
            "72ec1ef1124a45b047e8b7c75a932195135bb61de24ec0d1914042246e0aec3a2354e093d76f3048b456764346900cb130d2a4fd5dd16abb5e30bcb850dee843",
            "e718483d0ce769644e2e42c7bc15b4638e1f98b13b2044285632a803afa973ebde0ff244877ea60a4cb0432ce577c31beb009c5c2c49aa2e4eadb217ad8cc09b"}},
    };

    for (const auto& algorithm : algorithms) {
        for (int i = 0; i < 6; ++i) {
            if (!algorithm.expected[i]) continue;
            const auto& message = messages[i];
            const std::size_t chunks[] {message.size() ? message.size() : 1, 1, algorithm.blockSize - 1, algorithm.blockSize, algorithm.blockSize + 1, 1000};
            for (auto chunk : chunks) {
                if (i == 5 && chunk < 64) continue;
                Require(Digest(algorithm, message, chunk) == algorithm.expected[i], algorithm.name);
            }
        }

        alignas(16) unsigned char context[256] {};
        alignas(16) unsigned char copy[256] {};
        unsigned char digest[64] {};
        unsigned char copyDigest[64] {};
        const bool wide = algorithm.blockSize == 128;
        const auto num = [&](const unsigned char* c) {
            std::uint32_t value;
            std::memcpy(&value, c + (wide ? 208 : (algorithm.contextSize == 112 ? 104 : algorithm.contextSize - 4)), 4);
            return value;
        };
        const auto bitsLow = [&](const unsigned char* c) {
            const std::size_t offset = wide ? 64 : (algorithm.contextSize == 96 ? 20 : algorithm.contextSize == 112 ? 32 : 16);
            std::uint64_t value = 0;
            std::memcpy(&value, c + offset, wide ? 8 : 4);
            return value;
        };
        Require(algorithm.init(context) == 1, "init");
        Require(algorithm.update(context, nullptr, 0) == 1, "empty update with null data");
        Require(algorithm.update(context, "ab", 2) == 1, "partial update");
        Require(num(context) == 2 && bitsLow(context) == 16, "context records the buffered bytes and the bit count");
        std::memcpy(copy, context, sizeof(copy));
        Require(algorithm.update(context, "c", 1) == 1 && algorithm.update(copy, "c", 1) == 1, "update of a copied context");
        Require(algorithm.final(digest, context) == 1 && algorithm.final(copyDigest, copy) == 1, "final of a copied context");
        Require(Hex(digest, algorithm.digestSize) == algorithm.expected[1] && !std::memcmp(digest, copyDigest, sizeof(digest)), "copied context digest");
        if (!wide) Require(num(context) == 0, "final clears the buffered count");
        RequireThrows([&] { algorithm.init(nullptr); }, "null context");
        RequireThrows([&] { algorithm.update(context, nullptr, 1); }, "null data");
        if (!wide) RequireThrows([&] { algorithm.final(nullptr, context); }, "null digest");
    }

    alignas(16) unsigned char context[256] {};
    unsigned char digest[64] {};
    Require(SHA512_Init_nid_postfix(context) == 1 && SHA512_Final_nid_postfix(nullptr, context) == 0, "SHA512_Final without a digest buffer");
    SHA256_Init_nid_postfix(context);
    std::uint32_t length = 33;
    std::memcpy(context + 108, &length, 4);
    Require(SHA256_Final_nid_postfix(digest, context) == 0, "SHA256_Final with an oversized digest length");
    SHA256_Init_nid_postfix(context);
    length = 16;
    std::memcpy(context + 108, &length, 4);
    std::memset(digest, 0xee, sizeof(digest));
    Require(SHA256_Final_nid_postfix(digest, context) == 1 && Hex(digest, 16) == "e3b0c44298fc1c149afbf4c8996fb924" && digest[16] == 0xee, "SHA256_Final truncates to the digest length");
    for (std::uint32_t truncated : {28u, 32u}) {
        SHA512_Init_nid_postfix(context);
        std::memcpy(context + 212, &truncated, 4);
        std::memset(digest, 0xee, sizeof(digest));
        Require(SHA512_Final_nid_postfix(digest, context) == 0 && digest[0] == 0xee && digest[63] == 0xee, "SHA512_Final with a SHA-512/t digest length");
    }
    SHA256_Init_nid_postfix(context);
    length = 64;
    std::memcpy(context + 104, &length, 4);
    RequireThrows([&] { SHA256_Update_nid_postfix(context, "a", 1); }, "corrupt buffered count");

    const auto* abc = reinterpret_cast<const unsigned char*>("abc");
    Require(SHA256_nid_postfix(abc, 3, digest) == digest && Hex(digest, 32) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA256 one-shot");
    Require(SHA256_nid_postfix(nullptr, 0, digest) == digest && Hex(digest, 32) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "SHA256 one-shot of nothing");
    unsigned char* shared = SHA256_nid_postfix(abc, 3, nullptr);
    Require(shared && shared != digest && Hex(shared, 32) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA256 one-shot into the static buffer");
    Require(SHA256_nid_postfix(nullptr, 0, nullptr) == shared && Hex(shared, 32) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "SHA256 one-shot reuses the static buffer");
    std::puts("libSceLibreSsl digest tests passed");
}
