#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Ec.hpp"

#include <array>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

class ReadOnlyPage {
public:
    ReadOnlyPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        bytes = info.dwPageSize;
        data = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Require(data != nullptr, "read-only page allocation failed");
#else
        const long page = sysconf(_SC_PAGESIZE);
        Require(page > 0, "page size query failed");
        bytes = static_cast<std::size_t>(page);
        data = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(data != nullptr && data != MAP_FAILED, "read-only page allocation failed");
#endif
    }

    ~ReadOnlyPage() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, bytes);
#endif
    }

    template <typename T>
    T* Set(const T& value) {
        Require(sizeof(value) <= bytes, "read-only page too small");
        std::memcpy(data, &value, sizeof(value));
        return static_cast<T*>(data);
    }

    void Protect() {
#ifdef _WIN32
        DWORD previous{};
        Require(VirtualProtect(data, bytes, PAGE_READONLY, &previous) != 0, "read-only page protection failed");
#else
        Require(mprotect(data, bytes, PROT_READ) == 0, "read-only page protection failed");
#endif
    }

private:
    void* data = nullptr;
    std::size_t bytes = 0;
};

template <typename TCall>
static void Rejects(TCall call, const char* message) {
    errno = EINVAL;
    bool threw = false;
    try {
        call();
    } catch (const std::overflow_error&) {
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && errno == EINVAL, message);
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
    std::memcpy(method.data() + 104, &callback, sizeof(callback));
    key.type = 3072;
    errno = EINVAL;
    Require(EVP_PKEY_bits_nid_postfix(&key) == 3072 && errno == EINVAL, "bits changed errno");
    Rejects([] { EVP_PKEY_bits_nid_postfix(reinterpret_cast<const LibreSsl3EvpPkey*>(0x10)); }, "bits read an unmapped key");
    LibreSsl3EvpPkey unmappedMethod{6, 1, reinterpret_cast<const void*>(0x10), nullptr, 0};
    const int callsBefore = bitsCalls;
    Rejects([&] { EVP_PKEY_bits_nid_postfix(&unmappedMethod); }, "bits read an unmapped method");
    Require(bitsCalls == callsBefore, "bits called through an unmapped method");
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
    Rejects([&] { retain(reinterpret_cast<TKey*>(0x10)); }, "retain of an unmapped object did not reject");
    {
        TKey value;
        std::memset(&value, 0x6B, sizeof(value));
        value.references = 3;
        ReadOnlyPage page;
        TKey* readOnly = page.Set(value);
        page.Protect();
        Rejects([&] { retain(readOnly); }, "retain of a read-only object did not reject");
        Require(std::memcmp(readOnly, &value, sizeof(value)) == 0, "rejected retain changed a read-only object");
    }
    errno = EINVAL;
    guarded.key.references = 5;
    Require(retain(&guarded.key) == 1 && guarded.key.references == 6 && errno == EINVAL, "retain changed errno");
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

static_assert(sizeof(EcKey) == sizeof(LibreSsl3EcKey));
static_assert(offsetof(EcKey, references) == offsetof(LibreSsl3EcKey, references));

static LibreSsl3EcKey* Shared(EcKey* key) {
    return reinterpret_cast<LibreSsl3EcKey*>(key);
}

static void RequireP256(const EcKey* key, const char* message) {
    const EcGroup* group = EC_KEY_get0_group_nid_postfix(key);
    Require(group && EC_GROUP_get_degree_nid_postfix(group) == 256 && ECDSA_size_nid_postfix(key) == 72, message);
}

static void CheckEcLifetime(bool named) {
    EcKey* key = named ? EC_KEY_new_by_curve_name_nid_postfix(415) : EC_KEY_new_nid_postfix();
    Require(key && key->references == 1, "new EC key reference count");
    Require(key->version == 1 && key->conversionForm == 4, "new EC key version or conversion form");
    Require(!key->method && !key->publicKey && !key->privateKey && key->encodingFlags == 0 && key->flags == 0 && !key->extraData, "new EC key fields");
    if (!named) {
        Require(EC_KEY_get0_group_nid_postfix(key) == nullptr, "new EC key has a group");
        EcGroup* group = EC_GROUP_new_by_curve_name_nid_postfix(415);
        Require(group && EC_KEY_set_group_nid_postfix(key, group) == 1, "set EC key group");
        EC_GROUP_free_nid_postfix(group);
    }
    Require(EC_KEY_up_ref_nid_postfix(Shared(key)) == 1 && key->references == 2, "retain EC key");
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
                Require(EC_KEY_up_ref_nid_postfix(Shared(key)) == 1, "concurrent EC retain failed");
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

static void CheckGet0EcKey() {
    bool nullThrown = false;
    try {
        EVP_PKEY_get0_EC_KEY_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        nullThrown = true;
    }
    Require(nullThrown, "get0_EC_KEY of null did not throw");

    LibreSsl3EvpPkey key {};
    LibreSsl3EcKey ecKey {};
    key.key = &ecKey;

    for (const std::int32_t wrongType : {0, -1, 1, 6, 19, 28, 66, 116, 407, 409, 855, 894, 912}) {
        key.type = wrongType;
        LibreSsl3EvpPkey before;
        std::memcpy(&before, &key, sizeof(key));
        Require(EVP_PKEY_get0_EC_KEY_nid_postfix(&key) == nullptr, "get0_EC_KEY wrong type return non-null");
        Require(std::memcmp(&before, &key, sizeof(key)) == 0, "get0_EC_KEY mutated key");
    }

    key.type = 408;
    LibreSsl3EvpPkey before;
    std::memcpy(&before, &key, sizeof(key));
    Require(EVP_PKEY_get0_EC_KEY_nid_postfix(&key) == &ecKey, "get0_EC_KEY correct type return matching pointer");
    Require(std::memcmp(&before, &key, sizeof(key)) == 0, "get0_EC_KEY mutated key on success");

    key.key = nullptr;
    Require(EVP_PKEY_get0_EC_KEY_nid_postfix(&key) == nullptr, "get0_EC_KEY null inner key pointer");
    Rejects([] { EVP_PKEY_get0_EC_KEY_nid_postfix(reinterpret_cast<LibreSsl3EvpPkey*>(0x10)); }, "get0_EC_KEY read an unmapped key");
}

int main(int argc, char** argv) {
    if (argc == 1 || std::strcmp(argv[1], "evp") == 0)
        CheckKey<LibreSsl3EvpPkey>(EVP_PKEY_up_ref_nid_postfix, 4);
    if (argc == 1 || std::strcmp(argv[1], "ec") == 0) {
        CheckKey<LibreSsl3EcKey>(EC_KEY_up_ref_nid_postfix, 48);
        EC_KEY_free_nid_postfix(nullptr);
        CheckEcLifetime(false);
        CheckEcLifetime(true);
    }
    if (argc == 1 || std::strcmp(argv[1], "rsa") == 0)
        CheckKey<LibreSsl3Rsa>(RSA_up_ref_nid_postfix, 104);
    if (argc == 1 || std::strcmp(argv[1], "bits") == 0)
        CheckBits();
    if (argc == 1 || std::strcmp(argv[1], "ssl_ctx") == 0)
        CheckKey<LibreSsl3SslCtx>(SSL_CTX_up_ref_nid_postfix, 40);
    if (argc == 1 || std::strcmp(argv[1], "bio") == 0)
        CheckKey<LibreSsl::Bio>(BIO_up_ref_nid_postfix, 72);
    if (argc == 1 || std::strcmp(argv[1], "ec_get0") == 0)
        CheckGet0EcKey();
    return 0;
}
