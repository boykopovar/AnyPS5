#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"

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

static void RequireP256(const EcKey* key, const char* message) {
    const EcGroup* group = EC_KEY_get0_group_nid_postfix(key);
    Require(group && EC_GROUP_get_degree_nid_postfix(group) == 256 && ECDSA_size_nid_postfix(key) == 72, message);
}

static void CheckEcLifetime(bool named) {
    EcKey* key = named ? EC_KEY_new_by_curve_name_nid_postfix(415) : EC_KEY_new_nid_postfix();
    Require(key && key->references == 1, "new EC key reference count");
    if (!named) {
        Require(EC_KEY_get0_group_nid_postfix(key) == nullptr, "new EC key has a group");
        EcGroup* group = EC_GROUP_new_by_curve_name_nid_postfix(415);
        Require(group && EC_KEY_set_group_nid_postfix(key, group) == 1, "set EC key group");
        EC_GROUP_free_nid_postfix(group);
    }
    Require(EC_KEY_up_ref_nid_postfix(key) == 1 && key->references == 2, "retain EC key");
    EC_KEY_free_nid_postfix(key);
    Require(key->references == 1, "EC free did not release one reference");
    RequireP256(key, "retained EC key is unusable after free");
    std::atomic<bool> start = false;
    std::array<std::thread, 4> threads;
    for (auto& thread : threads) {
        thread = std::thread([&] {
            while (!start.load())
                std::this_thread::yield();
            for (int index = 0; index < 1000; ++index) {
                Require(EC_KEY_up_ref_nid_postfix(key) == 1, "concurrent EC retain failed");
                EC_KEY_free_nid_postfix(key);
            }
        });
    }
    start = true;
    for (auto& thread : threads)
        thread.join();
    Require(key->references == 1, "balanced concurrent EC references");
    RequireP256(key, "EC key is unusable after concurrent release");
    EC_KEY_free_nid_postfix(key);
}

static void CheckEcRetain() {
    EcKey key;
    for (std::int32_t initial : {-2, 0, 1, 37, std::numeric_limits<std::int32_t>::max() - 1}) {
        key.references = initial;
        Require(EC_KEY_up_ref_nid_postfix(&key) == (initial > 0), "EC retain return value");
        Require(key.references == initial + 1 && !key.group, "EC retain count");
    }
    key.references = std::numeric_limits<std::int32_t>::max();
    bool overflowThrown = false;
    try {
        EC_KEY_up_ref_nid_postfix(&key);
    } catch (const std::overflow_error&) {
        overflowThrown = true;
    }
    Require(overflowThrown && key.references == std::numeric_limits<std::int32_t>::max() && !key.group, "EC overflow retain must throw without mutation");
    bool nullThrown = false;
    try {
        EC_KEY_up_ref_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        nullThrown = true;
    }
    Require(nullThrown, "EC retain of null did not throw");
    EC_KEY_free_nid_postfix(nullptr);
    CheckEcLifetime(false);
    CheckEcLifetime(true);
}

int main(int argc, char** argv) {
    if (argc == 1 || std::strcmp(argv[1], "evp") == 0)
        CheckKey<LibreSsl3EvpPkey>(EVP_PKEY_up_ref_nid_postfix, 4);
    if (argc == 1 || std::strcmp(argv[1], "ec") == 0)
        CheckEcRetain();
    if (argc == 1 || std::strcmp(argv[1], "rsa") == 0)
        CheckKey<LibreSsl3Rsa>(RSA_up_ref_nid_postfix, 104);
    return 0;
}
