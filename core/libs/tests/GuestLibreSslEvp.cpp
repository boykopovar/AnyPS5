#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

struct Algorithm {
    const char* name;
    const EvpMd* md;
    int type;
    int pkeyType;
    int size;
    int block;
    int contextSize;
    std::uint64_t flags;
    const char* digests[3];
    const char* macs[6];
};

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL EVP check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

static int allocationsBeforeFailure = -1;
static int liveBlocks = 0;

static bool FailAllocation() {
    if (allocationsBeforeFailure < 0) return false;
    return allocationsBeforeFailure-- == 0;
}

static void* APS5_VABI Allocate(std::size_t bytes) {
    if (FailAllocation()) return nullptr;
    void* const block = std::malloc(bytes ? bytes : 1);
    if (block) ++liveBlocks;
    return block;
}

static void APS5_VABI Release(void* block) {
    if (block) --liveBlocks;
    std::free(block);
}

static void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) {
    if (FailAllocation()) return nullptr;
    void* const block = std::calloc(count ? count : 1, bytes ? bytes : 1);
    if (block) ++liveBlocks;
    return block;
}

static void* APS5_VABI Reallocate(void*, std::size_t) { return nullptr; }
static void* APS5_VABI Align(std::size_t, std::size_t) { return nullptr; }
static void* APS5_VABI Realign(void*, std::size_t, std::size_t) { return nullptr; }
static int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { return 12; }

static void InitializeHeap() {
    void* const api[10] {
        reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&AllocateZeroed),
        reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
        reinterpret_cast<void*>(&PosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(api);
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

static std::vector<unsigned char> Bytes(const char* text) {
    return std::vector<unsigned char>(text, text + std::strlen(text));
}

static bool AllZero(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    return std::all_of(bytes, bytes + size, [](unsigned char value) { return value == 0; });
}

template<typename T> static T Field(const void* base, std::size_t offset) {
    T value;
    std::memcpy(&value, static_cast<const unsigned char*>(base) + offset, sizeof(value));
    return value;
}

static std::string DigestInChunks(EvpMdContext& context, const EvpMd* md, const std::vector<unsigned char>& message, std::size_t chunk) {
    unsigned char digest[64] {};
    unsigned int size = 0;
    Require(EVP_DigestInit_ex_nid_postfix(&context, md, nullptr) == 1);
    for (std::size_t offset = 0; offset < message.size(); offset += chunk)
        Require(EVP_DigestUpdate_nid_postfix(&context, message.data() + offset, std::min(chunk, message.size() - offset)) == 1);
    Require(EVP_DigestFinal_ex_nid_postfix(&context, digest, &size) == 1);
    return Hex(digest, size);
}

static std::string MacInChunks(HmacContext* context, const std::vector<unsigned char>& key, const EvpMd* md, const std::vector<unsigned char>& message, std::size_t chunk) {
    unsigned char mac[64] {};
    unsigned int size = 0;
    Require(HMAC_Init_ex_nid_postfix(context, key.data(), static_cast<int>(key.size()), md, nullptr) == 1);
    for (std::size_t offset = 0; offset < message.size(); offset += chunk)
        Require(HMAC_Update_nid_postfix(context, message.data() + offset, std::min(chunk, message.size() - offset)) == 1);
    Require(HMAC_Final_nid_postfix(context, mac, &size) == 1);
    return Hex(mac, size);
}

static int customInits = 0;
static int customCleanups = 0;

static int APS5_VABI CustomInit(EvpMdContext* context) {
    ++customInits;
    std::memset(context->mdData, 0, 4);
    return 1;
}

static int APS5_VABI CustomUpdate(EvpMdContext* context, const void* data, std::size_t length) {
    auto* sum = static_cast<unsigned char*>(context->mdData);
    for (std::size_t i = 0; i < length; ++i) sum[i % 4] = static_cast<unsigned char>(sum[i % 4] + static_cast<const unsigned char*>(data)[i]);
    return 1;
}

static int APS5_VABI CustomFinal(EvpMdContext* context, unsigned char* digest) {
    std::memcpy(digest, context->mdData, 4);
    return 1;
}

static int APS5_VABI CustomCleanup(EvpMdContext*) {
    ++customCleanups;
    return 1;
}

int main() {
    InitializeHeap();
    const std::vector<unsigned char> empty;
    const auto abc = Bytes("abc");
    std::vector<unsigned char> longMessage(1000);
    for (std::size_t i = 0; i < longMessage.size(); ++i) longMessage[i] = static_cast<unsigned char>(i * 7 + 3);
    const std::vector<std::vector<unsigned char>> messages {empty, abc, longMessage};

    std::vector<std::pair<std::vector<unsigned char>, std::vector<unsigned char>>> macCases;
    macCases.emplace_back(std::vector<unsigned char>(20, 0x0b), Bytes("Hi There"));
    macCases.emplace_back(Bytes("Jefe"), Bytes("what do ya want for nothing?"));
    macCases.emplace_back(std::vector<unsigned char>(20, 0xaa), std::vector<unsigned char>(50, 0xdd));
    std::vector<unsigned char> counting(25);
    for (std::size_t i = 0; i < counting.size(); ++i) counting[i] = static_cast<unsigned char>(i + 1);
    macCases.emplace_back(counting, std::vector<unsigned char>(50, 0xcd));
    macCases.emplace_back(std::vector<unsigned char>(131, 0xaa), Bytes("Test Using Larger Than Block-Size Key - Hash Key First"));
    macCases.emplace_back(std::vector<unsigned char>(131, 0xaa), Bytes("This is a test using a larger than block-size key and a larger than block-size data. The key needs to be hashed before being used by the HMAC algorithm."));

    const Algorithm algorithms[] {
        {"MD5", EVP_md5_nid_postfix(), 4, 8, 16, 64, 100, 0,
         {"d41d8cd98f00b204e9800998ecf8427e", "900150983cd24fb0d6963f7d28e17f72", "10046f077f2082ac19676b8079f1cb1a"},
         {"5ccec34ea9656392457fa1ac27f08fbc", "750c783e6ab0b503eaa86e310a5db738", "2ab8b9a9f7d3894d15ad8383b97044b2",
          "697eaf0aca3a3aea3a75164746ffaa79", "bfecaf4efff90a3a668f3922fec3762d", "09b8ae7b15adbbb243aca3491b51512b"}},
        {"SHA256", EVP_sha256_nid_postfix(), 672, 668, 32, 64, 120, 0xc,
         {"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "1e9bc38cbf860b9ec31918b065f9b52476c549a782e0e7990bed8ce3868d2371"},
         {"b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7", "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843",
          "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe", "82558a389a443c0ea4cc819899f2083a85f0faa3e578f8077a2e3ff46729665b",
          "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54", "9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2"}},
        {"SHA384", EVP_sha384_nid_postfix(), 673, 669, 48, 128, 224, 0xc,
         {"38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b",
          "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7",
          "94c38db521ca733b8904c2d14b6e82d33dcfcc26e1318c579dbee1fc2f472019034e792263b9d46e90e700de2f6e7e91"},
         {"afd03944d84895626b0825f4ab46907f15f9dadbe4101ec682aa034c7cebc59cfaea9ea9076ede7f4af152e8b2fa9cb6",
          "af45d2e376484031617f78d2b58a6b1b9c7ef464f5a01b47e42ec3736322445e8e2240ca5e69e2c78b3239ecfab21649",
          "88062608d3e6ad8a0aa2ace014c8a86f0aa635d947ac9febe83ef4e55966144b2a5ab39dc13814b94e3ab6e101a34f27",
          "3e8a69b7783c25851933ab6290af6ca77a9981480850009cc5577c6e1f573b4e6801dd23c4a7d679ccf8a386c674cffb",
          "4ece084485813e9088d2c63a041bc5b44f9ef1012a2b588f3cd11f05033ac4c60c2ef6ab4030fe8296248df163f44952",
          "6617178e941f020d351e2f254e8fd32c602420feb0b8fb9adccebb82461e99c5a678cc31e799176d3860e6110c46523e"}},
        {"SHA512", EVP_sha512_nid_postfix(), 674, 670, 64, 128, 224, 0xc,
         {"cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e",
          "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f",
          "00e36fccf193e59697a92b5ab24666ce6326d7fa16bf10832d0991ddc591112e9dfa6a636950ed9c4d67344a760654c2ff7785e1d60094d651038735b5dccabd"},
         {"87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b30545e17cdedaa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f1702e696c203a126854",
          "164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea2505549758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737",
          "fa73b0089d56a284efb0f0756c890be9b1b5dbdd8ee81a3655f83e33b2279d39bf3e848279a722c806b485a47e67c807b946a337bee8942674278859e13292fb",
          "b0ba465637458c6990e5a8c5f61d4af7e576d97ff94b872de76f8050361ee3dba91ca5c11aa25eb4d679275cc5788063a5f19741120c4f2de2adebeb10a298dd",
          "80b24263c7c1a3ebb71493c1dd7be8b49b46d1f41b4aeec1121b013783f8f3526b56d037e05f2598bd0fd2215d6a1e5295e64f73f63f0aec8b915a985d786598",
          "e37b6a775dc87dbaa4dfa9f96e5e3ffddebd71f8867289865df5a32d20cdc944b6022cac3c4982b10d5eeb55c3e4de15134676fb6de0446065c97440fa8c6a58"}},
    };

    Require(EVP_MD_size_nid_postfix(nullptr) == -1);
    Require(EVP_md5_nid_postfix() == algorithms[0].md && EVP_sha512_nid_postfix() == algorithms[3].md);
    for (std::size_t i = 0; i < 4; ++i)
        for (std::size_t j = i + 1; j < 4; ++j) Require(algorithms[i].md != algorithms[j].md);
    for (const auto& algorithm : algorithms) {
        Require(algorithm.md != nullptr);
        Require(EVP_MD_size_nid_postfix(algorithm.md) == algorithm.size);
        Require(Field<std::int32_t>(algorithm.md, 0) == algorithm.type);
        Require(Field<std::int32_t>(algorithm.md, 4) == algorithm.pkeyType);
        Require(Field<std::int32_t>(algorithm.md, 8) == algorithm.size);
        Require(Field<std::uint64_t>(algorithm.md, 16) == algorithm.flags);
        Require(Field<std::int32_t>(algorithm.md, 80) == 6 && Field<std::int32_t>(algorithm.md, 84) == 19);
        Require(Field<std::int32_t>(algorithm.md, 100) == algorithm.block);
        Require(Field<std::int32_t>(algorithm.md, 104) == algorithm.contextSize);
        Require(Field<void*>(algorithm.md, 24) && Field<void*>(algorithm.md, 32) && Field<void*>(algorithm.md, 40));
        Require(Field<void*>(algorithm.md, 48) == nullptr && Field<void*>(algorithm.md, 56) == nullptr);

        auto* created = EVP_MD_CTX_create_nid_postfix();
        Require(created && AllZero(created, sizeof(EvpMdContext)));
        for (std::size_t m = 0; m < messages.size(); ++m) {
            for (const std::size_t chunk : {std::size_t(1), std::size_t(algorithm.block - 1), std::size_t(algorithm.block), std::size_t(algorithm.block + 1), std::size_t(1000)})
                Require(DigestInChunks(*created, algorithm.md, messages[m], chunk) == algorithm.digests[m]);
            unsigned char digest[64] {};
            unsigned int size = 0;
            Require(EVP_Digest_nid_postfix(messages[m].data(), messages[m].size(), digest, &size, algorithm.md, nullptr) == 1);
            Require(size == static_cast<unsigned int>(algorithm.size) && Hex(digest, size) == algorithm.digests[m]);
            Require(EVP_Digest_nid_postfix(messages[m].data(), messages[m].size(), digest, nullptr, algorithm.md, nullptr) == 1);
        }
        Require(created->digest == algorithm.md && Field<void*>(created, 40) == Field<void*>(algorithm.md, 32));
        Require(created->mdData && AllZero(created->mdData, static_cast<std::size_t>(algorithm.contextSize)));
        void* const data = created->mdData;
        Require(EVP_DigestInit_ex_nid_postfix(created, algorithm.md, nullptr) == 1 && created->mdData == data);
        Require(EVP_MD_CTX_cleanup_nid_postfix(created) == 1 && AllZero(created, sizeof(EvpMdContext)));
        EVP_MD_CTX_destroy_nid_postfix(created);

        HmacContext hmac;
        const auto* hmacBytes = reinterpret_cast<const unsigned char*>(&hmac);
        std::memset(&hmac, 0x5a, sizeof(hmac));
        HMAC_CTX_init_nid_postfix(&hmac);
        Require(Field<const void*>(&hmac, 0) == nullptr && AllZero(hmacBytes + 8, 3 * sizeof(EvpMdContext)));
        for (std::size_t c = 0; c < macCases.size(); ++c) {
            const auto& [key, message] = macCases[c];
            for (const std::size_t chunk : {std::size_t(1), std::size_t(algorithm.block), std::size_t(1000)})
                Require(MacInChunks(&hmac, key, algorithm.md, message, chunk) == algorithm.macs[c]);
            Require(Field<const void*>(&hmac, 0) == algorithm.md);
            const bool hashed = key.size() > static_cast<std::size_t>(algorithm.block);
            const auto keyLength = Field<std::uint32_t>(&hmac, 152);
            Require(keyLength == (hashed ? static_cast<std::uint32_t>(algorithm.size) : key.size()));
            Require(hashed || std::memcmp(hmacBytes + 156, key.data(), key.size()) == 0);
            Require(AllZero(hmacBytes + 156 + keyLength, 128 - keyLength));

            unsigned char mac[64] {};
            unsigned int size = 0;
            Require(HMAC_Init_ex_nid_postfix(&hmac, nullptr, 0, nullptr, nullptr) == 1);
            Require(HMAC_Update_nid_postfix(&hmac, message.data(), message.size()) == 1);
            Require(HMAC_Final_nid_postfix(&hmac, mac, &size) == 1 && Hex(mac, size) == algorithm.macs[c]);
            Require(HMAC_Init_ex_nid_postfix(&hmac, nullptr, 0, algorithm.md, nullptr) == 1);
            Require(HMAC_Update_nid_postfix(&hmac, message.data(), message.size()) == 1);
            Require(HMAC_Final_nid_postfix(&hmac, mac, nullptr) == 1 && Hex(mac, static_cast<std::size_t>(algorithm.size)) == algorithm.macs[c]);

            std::memset(mac, 0, sizeof(mac));
            size = 0;
            Require(HMAC_nid_postfix(algorithm.md, key.data(), static_cast<int>(key.size()), message.data(), message.size(), mac, &size) == mac);
            Require(size == static_cast<unsigned int>(algorithm.size) && Hex(mac, size) == algorithm.macs[c]);
            const unsigned char* shared = HMAC_nid_postfix(algorithm.md, key.data(), static_cast<int>(key.size()), message.data(), message.size(), nullptr, nullptr);
            Require(shared && Hex(shared, static_cast<std::size_t>(algorithm.size)) == algorithm.macs[c]);
        }
        HMAC_CTX_cleanup_nid_postfix(&hmac);
        Require(AllZero(&hmac, sizeof(hmac)));
    }

    const auto* sha256 = EVP_sha256_nid_postfix();
    const auto* sha512 = EVP_sha512_nid_postfix();
    const auto* md5 = EVP_md5_nid_postfix();

    EvpMdContext context;
    std::memset(&context, 0x77, sizeof(context));
    EVP_MD_CTX_init_nid_postfix(&context);
    Require(AllZero(&context, sizeof(context)));
    Require(EVP_DigestInit_ex_nid_postfix(&context, nullptr, nullptr) == 0);
    RequireThrows([&] { EVP_DigestUpdate_nid_postfix(&context, abc.data(), abc.size()); });
    RequireThrows([&] { EVP_DigestFinal_ex_nid_postfix(&context, nullptr, nullptr); });
    Require(EVP_DigestInit_ex_nid_postfix(&context, sha256, nullptr) == 1);
    Require(EVP_DigestInit_ex_nid_postfix(&context, sha512, nullptr) == 1 && context.digest == sha512);
    Require(DigestInChunks(context, sha512, abc, 2) == algorithms[3].digests[1]);
    RequireThrows([&] { EVP_DigestInit_ex_nid_postfix(&context, nullptr, nullptr); });
    int engine = 0;
    RequireThrows([&] { EVP_DigestInit_ex_nid_postfix(&context, sha256, &engine); });
    RequireThrows([&] { EVP_Digest_nid_postfix(abc.data(), abc.size(), nullptr, nullptr, sha256, &engine); });
    RequireThrows([&] { EVP_DigestUpdate_nid_postfix(&context, nullptr, 1); });
    RequireThrows([&] { EVP_DigestUpdate_nid_postfix(nullptr, abc.data(), abc.size()); });
    RequireThrows([&] { EVP_MD_CTX_init_nid_postfix(nullptr); });
    RequireThrows([&] { EVP_MD_CTX_cleanup_nid_postfix(nullptr); });
    EVP_MD_CTX_destroy_nid_postfix(nullptr);
    context.engine = &engine;
    RequireThrows([&] { EVP_MD_CTX_cleanup_nid_postfix(&context); });
    context.engine = nullptr;
    context.pkeyContext = &engine;
    RequireThrows([&] { EVP_DigestInit_ex_nid_postfix(&context, sha512, nullptr); });
    context.pkeyContext = nullptr;
    Require(EVP_MD_CTX_cleanup_nid_postfix(&context) == 1);

    EVP_MD_CTX_init_nid_postfix(&context);
    context.flags = 0x100;
    Require(EVP_DigestInit_ex_nid_postfix(&context, sha256, nullptr) == 1);
    Require(context.digest == sha256 && context.mdData == nullptr && context.update == nullptr);
    Require(EVP_MD_CTX_cleanup_nid_postfix(&context) == 1 && AllZero(&context, sizeof(context)));

    EvpMd custom {};
    custom.type = 12345;
    custom.mdSize = 4;
    custom.init = CustomInit;
    custom.update = CustomUpdate;
    custom.finalize = CustomFinal;
    custom.cleanup = CustomCleanup;
    custom.blockSize = 64;
    custom.contextSize = 16;
    unsigned char sum[4] {};
    unsigned int sumSize = 0;
    const unsigned char bytes[] {1, 2, 3, 4, 5, 6, 7, 8, 9};
    Require(EVP_Digest_nid_postfix(bytes, sizeof(bytes), sum, &sumSize, &custom, nullptr) == 1);
    Require(sumSize == 4 && sum[0] == 15 && sum[1] == 8 && sum[2] == 10 && sum[3] == 12);
    Require(customInits == 1 && customCleanups == 1);
    Require(EVP_MD_size_nid_postfix(&custom) == 4);
    custom.mdSize = 65;
    Require(EVP_Digest_nid_postfix(bytes, sizeof(bytes), sum, &sumSize, &custom, nullptr) == 0);

    HmacContext hmac {};
    const auto* hmacBytes = reinterpret_cast<const unsigned char*>(&hmac);
    HMAC_CTX_init_nid_postfix(&hmac);
    unsigned char mac[64] {};
    unsigned int macSize = 0;
    Require(HMAC_Update_nid_postfix(&hmac, abc.data(), abc.size()) == 0);
    Require(HMAC_Final_nid_postfix(&hmac, mac, &macSize) == 0);
    Require(HMAC_Init_ex_nid_postfix(&hmac, nullptr, 0, nullptr, nullptr) == 0);
    Require(HMAC_Init_ex_nid_postfix(&hmac, nullptr, 0, sha256, nullptr) == 0);
    Require(HMAC_Init_ex_nid_postfix(&hmac, "k", -1, sha256, nullptr) == 0);
    Require(HMAC_Init_ex_nid_postfix(&hmac, "Jefe", 4, sha256, nullptr) == 1);
    Require(HMAC_Init_ex_nid_postfix(&hmac, nullptr, 0, md5, nullptr) == 0);
    Require(HMAC_Init_ex_nid_postfix(&hmac, "Jefe", -1, nullptr, nullptr) == 0);
    Require(HMAC_Init_ex_nid_postfix(&hmac, "Jefe", 4, nullptr, nullptr) == 1);
    const auto question = Bytes("what do ya want for nothing?");
    Require(HMAC_Update_nid_postfix(&hmac, question.data(), question.size()) == 1);
    Require(HMAC_Final_nid_postfix(&hmac, mac, &macSize) == 1 && Hex(mac, macSize) == algorithms[1].macs[1]);
    RequireThrows([&] { HMAC_Init_ex_nid_postfix(&hmac, "Jefe", 4, sha256, &engine); });
    HMAC_CTX_cleanup_nid_postfix(&hmac);

    std::memset(&hmac, 0x33, sizeof(hmac));
    Require(HMAC_Init_nid_postfix(&hmac, "Jefe", 4, md5) == 1);
    Require(HMAC_Update_nid_postfix(&hmac, question.data(), question.size()) == 1);
    Require(HMAC_Final_nid_postfix(&hmac, mac, &macSize) == 1 && Hex(mac, macSize) == algorithms[0].macs[1]);
    Require(HMAC_Init_nid_postfix(&hmac, nullptr, 0, nullptr) == 1);
    Require(HMAC_Update_nid_postfix(&hmac, question.data(), question.size()) == 1);
    Require(HMAC_Final_nid_postfix(&hmac, mac, &macSize) == 1 && Hex(mac, macSize) == algorithms[0].macs[1]);
    HMAC_CTX_cleanup_nid_postfix(&hmac);

    std::vector<unsigned char> blockKey(128);
    for (std::size_t i = 0; i < blockKey.size(); ++i) blockKey[i] = static_cast<unsigned char>(i * 3 + 1);
    const struct { const EvpMd* md; std::size_t keySize; const char* mac; } blockKeyCases[] {
        {sha256, 64, "631102ef12fb140c75c4a32ec732a95ce65da9a6603779445246ba02f9df465e"},
        {sha512, 128, "4638cabcab823870028a23978c0291c3c51861040154631b99929a35e84aa6f5eebf2d3b0081ae6a45dd8241f0175302562bdf3f83b299af324bb54095207b03"},
    };
    for (const auto& [md, keySize, expected] : blockKeyCases) {
        const std::vector<unsigned char> key(blockKey.begin(), blockKey.begin() + static_cast<std::ptrdiff_t>(keySize));
        HMAC_CTX_init_nid_postfix(&hmac);
        for (const std::size_t chunk : {std::size_t(1), keySize, std::size_t(1000)}) {
            Require(MacInChunks(&hmac, key, md, longMessage, chunk) == expected);
            Require(Field<std::uint32_t>(&hmac, 152) == keySize && std::memcmp(hmacBytes + 156, key.data(), keySize) == 0);
        }
        HMAC_CTX_cleanup_nid_postfix(&hmac);
        Require(HMAC_nid_postfix(md, key.data(), static_cast<int>(keySize), longMessage.data(), longMessage.size(), mac, &macSize) == mac);
        Require(Hex(mac, macSize) == expected);
    }

    Require(HMAC_nid_postfix(sha256, nullptr, 0, abc.data(), abc.size(), mac, &macSize) == nullptr);
    Require(HMAC_nid_postfix(sha256, "", 0, nullptr, 0, mac, &macSize) == mac && macSize == 32);
    Require(Hex(mac, macSize) == "b613679a0814d9ec772f95d778c35fc5ff1697c493715653c6c712144292c5ad");
    RequireThrows([&] { HMAC_CTX_init_nid_postfix(nullptr); });
    RequireThrows([&] { HMAC_Update_nid_postfix(nullptr, abc.data(), abc.size()); });
    Require(liveBlocks == 0);

    EVP_MD_CTX_init_nid_postfix(&context);
    allocationsBeforeFailure = 0;
    Require(EVP_DigestInit_ex_nid_postfix(&context, sha256, nullptr) == 0);
    Require(AllZero(&context, sizeof(context)) && liveBlocks == 0);
    RequireThrows([&] { EVP_DigestUpdate_nid_postfix(&context, abc.data(), abc.size()); });
    RequireThrows([&] { EVP_DigestFinal_ex_nid_postfix(&context, mac, &macSize); });
    Require(DigestInChunks(context, sha256, abc, 1) == algorithms[1].digests[1] && liveBlocks == 1);
    allocationsBeforeFailure = 0;
    Require(EVP_DigestInit_ex_nid_postfix(&context, sha512, nullptr) == 0);
    Require(AllZero(&context, sizeof(context)) && liveBlocks == 0);
    Require(DigestInChunks(context, sha512, abc, 1) == algorithms[3].digests[1] && liveBlocks == 1);
    Require(EVP_MD_CTX_cleanup_nid_postfix(&context) == 1 && liveBlocks == 0);

    allocationsBeforeFailure = 0;
    Require(EVP_MD_CTX_create_nid_postfix() == nullptr && liveBlocks == 0);
    allocationsBeforeFailure = 0;
    Require(EVP_Digest_nid_postfix(abc.data(), abc.size(), mac, &macSize, sha256, nullptr) == 0 && liveBlocks == 0);
    Require(EVP_Digest_nid_postfix(abc.data(), abc.size(), mac, &macSize, sha256, nullptr) == 1 && Hex(mac, macSize) == algorithms[1].digests[1]);

    for (const std::size_t c : {std::size_t(1), std::size_t(4)}) {
        const auto& [key, message] = macCases[c];
        for (int failure = 0; failure < 3; ++failure) {
            HMAC_CTX_init_nid_postfix(&hmac);
            allocationsBeforeFailure = failure;
            Require(HMAC_Init_ex_nid_postfix(&hmac, key.data(), static_cast<int>(key.size()), sha256, nullptr) == 0);
            Require(allocationsBeforeFailure == -1 && liveBlocks == failure);
            for (const std::size_t offset : {std::size_t(8), std::size_t(56), std::size_t(104)})
                Require(Field<void*>(&hmac, offset + 24) || (!Field<const void*>(&hmac, offset) && !Field<void*>(&hmac, offset + 40)));
            if (key.size() <= 64) RequireThrows([&] { HMAC_Update_nid_postfix(&hmac, message.data(), message.size()); });
            Require(MacInChunks(&hmac, key, sha256, message, 1000) == algorithms[1].macs[c] && liveBlocks == 3);
            HMAC_CTX_cleanup_nid_postfix(&hmac);
            Require(liveBlocks == 0);
        }
        allocationsBeforeFailure = 2;
        Require(HMAC_nid_postfix(sha256, key.data(), static_cast<int>(key.size()), message.data(), message.size(), mac, &macSize) == nullptr);
        Require(allocationsBeforeFailure == -1 && liveBlocks == 0);
    }
    return 0;
}
