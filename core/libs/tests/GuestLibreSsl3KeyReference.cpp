#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
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

static std::map<void*, std::size_t> live;
static std::size_t allocationCount;
static std::size_t releaseCount;

static void* APS5_VABI Allocate(std::size_t bytes) {
    void* pointer = std::malloc(bytes ? bytes : 1);
    Require(pointer != nullptr, "guest allocation failed");
    Require(live.emplace(pointer, bytes).second, "guest allocation is already live");
    ++allocationCount;
    return pointer;
}

static void APS5_VABI Release(void* pointer) {
    if (!pointer) return;
    Require(live.erase(pointer) == 1, "guest allocation freed more than once");
    ++releaseCount;
    std::free(pointer);
}

static void* APS5_VABI AllocateZeroed(std::size_t, std::size_t) {
    throw std::runtime_error("unexpected guest calloc");
}

static void* APS5_VABI Reallocate(void*, std::size_t) {
    throw std::runtime_error("unexpected guest realloc");
}

static void* APS5_VABI Align(std::size_t, std::size_t) {
    throw std::runtime_error("unexpected guest memalign");
}

static void* APS5_VABI Realign(void*, std::size_t, std::size_t) {
    throw std::runtime_error("unexpected guest reallocalign");
}

static int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) {
    throw std::runtime_error("unexpected guest posix_memalign");
}

static void InitializeHeap() {
    void* const api[10] {reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Release), reinterpret_cast<void*>(AllocateZeroed),
        reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align), reinterpret_cast<void*>(Realign), reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api);
}

static void CheckEcLifetime(bool named) {
    Require(live.empty(), "guest allocations remain before EC lifetime test");
    allocationCount = 0;
    releaseCount = 0;
    EcKey* key = named ? EC_KEY_new_by_curve_name_nid_postfix(415) : EC_KEY_new_nid_postfix();
    Require(key && key->references == 1, "new EC key reference count");
    if (!named) {
        Require(EC_KEY_get0_group_nid_postfix(key) == nullptr, "new EC key has a group");
        EcGroup* group = EC_GROUP_new_by_curve_name_nid_postfix(415);
        Require(group && EC_KEY_set_group_nid_postfix(key, group) == 1, "set EC key group");
        EC_GROUP_free_nid_postfix(group);
    }
    Require(EC_KEY_generate_key_nid_postfix(key) == 1, "generate EC key");
    Require(EC_KEY_check_key_nid_postfix(key) == 1, "generated EC key is invalid");
    Require(allocationCount == 2 && live.size() == 2 && releaseCount == 0, "generated EC private key allocations");
    Require(EC_KEY_up_ref_nid_postfix(key) == 1 && key->references == 2, "retain real EC key");
    EC_KEY_free_nid_postfix(key);
    Require(releaseCount == 0 && live.size() == 2, "EC free destroyed a retained key");
    Require(key->references == 1, "EC free did not release a reference");
    Require(EC_KEY_check_key_nid_postfix(key) == 1, "retained EC key is unusable after free");
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
    Require(releaseCount == 0 && live.size() == 2, "concurrent EC free destroyed a held key");
    Require(EC_KEY_check_key_nid_postfix(key) == 1, "EC key is unusable after concurrent release");
    EC_KEY_free_nid_postfix(key);
    Require(releaseCount == 2 && live.empty(), "final EC free did not release both private key allocations exactly once");
}

static void CheckEcConcurrentFinalRelease() {
    Require(live.empty(), "guest allocations remain before concurrent final release");
    allocationCount = 0;
    releaseCount = 0;
    EcKey* key = EC_KEY_new_by_curve_name_nid_postfix(415);
    Require(key && EC_KEY_generate_key_nid_postfix(key) == 1, "generate EC key for concurrent final release");
    Require(EC_KEY_up_ref_nid_postfix(key) == 1 && key->references == 2, "two references before concurrent final release");
    Require(allocationCount == 2 && live.size() == 2 && releaseCount == 0, "private key allocations before concurrent final release");
    std::atomic<bool> start = false;
    std::array<std::thread, 2> threads;
    for (auto& thread : threads) {
        thread = std::thread([&] {
            while (!start.load())
                std::this_thread::yield();
            EC_KEY_free_nid_postfix(key);
        });
    }
    start = true;
    for (auto& thread : threads)
        thread.join();
    Require(releaseCount == 2 && live.empty(), "concurrent final EC release did not free each private key allocation once");
}

static void CheckEcRetain() {
    EcKey* key = EC_KEY_new_nid_postfix();
    Require(key != nullptr, "new EC retain test key");
    for (std::int32_t initial : {-2, 0, 1, 37, std::numeric_limits<std::int32_t>::max() - 1}) {
        key->references = initial;
        Require(EC_KEY_up_ref_nid_postfix(key) == (initial > 0), "EC retain return value");
        Require(key->references == initial + 1, "EC retain count");
        Require(!key->group && !key->publicKey && !key->privateKey, "EC retain changed key state");
    }
    key->references = std::numeric_limits<std::int32_t>::max();
    bool overflowThrown = false;
    try {
        EC_KEY_up_ref_nid_postfix(key);
    } catch (const std::overflow_error&) {
        overflowThrown = true;
    }
    Require(overflowThrown && key->references == std::numeric_limits<std::int32_t>::max(), "EC overflow retain must throw without mutation");
    Require(!key->group && !key->publicKey && !key->privateKey, "EC overflow retain changed key state");
    bool nullThrown = false;
    try {
        EC_KEY_up_ref_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        nullThrown = true;
    }
    Require(nullThrown, "EC retain of null did not throw");
    key->references = 1;
    EC_KEY_free_nid_postfix(key);
    EC_KEY_free_nid_postfix(nullptr);
    InitializeHeap();
    CheckEcLifetime(false);
    CheckEcLifetime(true);
    CheckEcConcurrentFinalRelease();
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
