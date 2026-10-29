#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <thread>

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
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
    return 0;
}
