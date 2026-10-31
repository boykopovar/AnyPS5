#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <thread>

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

using PkeyBitsFunction = int (APS5_VABI *)(const LibreSsl3EvpPkey*);

static const LibreSsl3EvpPkey* expectedBitsKey;
static int bitsCalls;

static int APS5_VABI BitsFromKey(const LibreSsl3EvpPkey* key) {
    Require(key == expectedBitsKey, "bits callback received the wrong key");
    ++bitsCalls;
    return key->type;
}

static int APS5_VABI WrongMethod(const LibreSsl3EvpPkey*) {
    Require(false, "bits called a neighboring method slot");
    return 0;
}

static void CheckBits() {
    Require(EVP_PKEY_bits_nid_postfix(nullptr) == 0, "null key bit count");
    LibreSsl3EvpPkey key{6, 17, nullptr, reinterpret_cast<void*>(1), 1};
    std::array<unsigned char, sizeof(key)> before;
    std::memcpy(before.data(), &key, sizeof(key));
    Require(EVP_PKEY_bits_nid_postfix(&key) == 0, "key without a method bit count");
    Require(std::memcmp(before.data(), &key, sizeof(key)) == 0, "bits changed a key without a method");
    alignas(8) std::array<unsigned char, 112> method{};
    const PkeyBitsFunction wrong = WrongMethod;
    std::memcpy(method.data() + 96, &wrong, sizeof(wrong));
    key.method = method.data();
    std::memcpy(before.data(), &key, sizeof(key));
    auto unchangedMethod = method;
    Require(EVP_PKEY_bits_nid_postfix(&key) == 0, "key without a bits callback bit count");
    Require(std::memcmp(before.data(), &key, sizeof(key)) == 0, "bits changed a key without a callback");
    Require(method == unchangedMethod, "bits changed a method without a callback");
    const PkeyBitsFunction callback = BitsFromKey;
    std::memcpy(method.data() + 104, &callback, sizeof(callback));
    unchangedMethod = method;
    expectedBitsKey = &key;
    for (int result : {0, 1, 255, 256, 2048, -7, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        key.type = result;
        std::memcpy(before.data(), &key, sizeof(key));
        const int calls = bitsCalls;
        Require(EVP_PKEY_bits_nid_postfix(&key) == result, "bits did not preserve the callback result");
        Require(bitsCalls == calls + 1, "bits did not call the callback exactly once");
        Require(std::memcmp(before.data(), &key, sizeof(key)) == 0, "bits changed the key");
        Require(method == unchangedMethod, "bits changed the key method");
    }
    const PkeyBitsFunction missing = nullptr;
    std::memcpy(method.data() + 104, &missing, sizeof(missing));
    const int calls = bitsCalls;
    Require(EVP_PKEY_bits_nid_postfix(&key) == 0, "bits retained a removed callback");
    Require(bitsCalls == calls, "bits called a removed callback");
}

template<class TKey, class TRetain>
static void CheckKey(TRetain retain, std::size_t referenceOffset) {
    struct GuardedKey {
        std::array<std::uint64_t, 2> before;
        TKey key;
        std::array<std::uint64_t, 2> after;
    } guarded;
    for (std::int32_t initial : {-2, 0, 1, 37, std::numeric_limits<std::int32_t>::max() - 1}) {
        std::memset(&guarded, 0xA5, sizeof(guarded));
        guarded.key.references = initial;
        std::array<unsigned char, sizeof(guarded)> expected;
        std::memcpy(expected.data(), &guarded, sizeof(guarded));
        const std::int32_t next = initial + 1;
        std::memcpy(expected.data() + 16 + referenceOffset, &next, sizeof(next));
        Require(retain(&guarded.key) == (next > 1), "retain return value");
        Require(std::memcmp(&guarded, expected.data(), sizeof(guarded)) == 0, "retain changed bytes outside reference count");
    }
    std::memset(&guarded, 0x3C, sizeof(guarded));
    guarded.key.references = std::numeric_limits<std::int32_t>::max();
    std::array<unsigned char, sizeof(guarded)> unchanged;
    std::memcpy(unchanged.data(), &guarded, sizeof(guarded));
    bool overflowThrown = false;
    try {
        retain(&guarded.key);
    } catch (const std::overflow_error&) {
        overflowThrown = true;
    }
    Require(overflowThrown, "retain at maximum count did not throw");
    Require(std::memcmp(&guarded, unchanged.data(), sizeof(guarded)) == 0, "overflowing retain changed bytes");
    bool nullThrown = false;
    try {
        retain(nullptr);
    } catch (const std::invalid_argument&) {
        nullThrown = true;
    }
    Require(nullThrown, "retain of null did not throw");
    std::memset(&guarded, 0x5A, sizeof(guarded));
    guarded.key.references = 1;
    std::array<unsigned char, sizeof(guarded)> expected;
    std::memcpy(expected.data(), &guarded, sizeof(guarded));
    for (int index = 0; index < 32; ++index)
        Require(retain(&guarded.key) == 1, "repeated retain failed");
    Require(guarded.key.references == 33, "repeated retain count");
    guarded.key.references = 1;
    std::atomic<bool> start = false;
    std::array<std::thread, 4> threads;
    for (auto& thread : threads) {
        thread = std::thread([&] {
            while (!start.load())
                std::this_thread::yield();
            for (int index = 0; index < 1000; ++index)
                Require(retain(&guarded.key) == 1, "concurrent retain failed");
        });
    }
    start = true;
    for (auto& thread : threads)
        thread.join();
    const std::int32_t finalCount = 4001;
    std::memcpy(expected.data() + 16 + referenceOffset, &finalCount, sizeof(finalCount));
    Require(std::memcmp(&guarded, expected.data(), sizeof(guarded)) == 0, "concurrent count or adjacent bytes changed");
}

int main(int argc, char** argv) {
    if (argc == 1 || std::strcmp(argv[1], "evp") == 0)
        CheckKey<LibreSsl3EvpPkey>(EVP_PKEY_up_ref_nid_postfix, 4);
    if (argc == 1 || std::strcmp(argv[1], "ec") == 0)
        CheckKey<LibreSsl3EcKey>(EC_KEY_up_ref_nid_postfix, 48);
    if (argc == 1 || std::strcmp(argv[1], "rsa") == 0)
        CheckKey<LibreSsl3Rsa>(RSA_up_ref_nid_postfix, 104);
    if (argc == 1 || std::strcmp(argv[1], "bits") == 0)
        CheckBits();
    return 0;
}
