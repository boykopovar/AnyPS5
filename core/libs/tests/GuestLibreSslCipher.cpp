#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

struct alignas(8) RawCipher {
    std::array<std::byte, 80> prefix;
    std::int32_t strength;
    std::int32_t algorithm;
};

static_assert(sizeof(RawCipher) == 88);
static_assert(offsetof(RawCipher, strength) == 80);
static_assert(offsetof(RawCipher, algorithm) == 84);

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

static void RequireString(const char* actual, const char* expected) {
    Require(actual && std::strcmp(actual, expected) == 0, expected);
}

class ReadOnlyPage {
public:
    ReadOnlyPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        bytes = info.dwPageSize;
        data = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Require(data != nullptr, "page alloc");
#else
        const long page = sysconf(_SC_PAGESIZE);
        Require(page > 0, "page size");
        bytes = static_cast<std::size_t>(page);
        data = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(data != nullptr && data != MAP_FAILED, "page mmap");
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
        Require(sizeof(value) <= bytes, "value fits page");
        std::memcpy(data, &value, sizeof(value));
        return static_cast<T*>(data);
    }

    void Protect() {
#ifdef _WIN32
        DWORD previous{};
        Require(VirtualProtect(data, bytes, PAGE_READONLY, &previous) != 0, "VirtualProtect");
#else
        Require(mprotect(data, bytes, PROT_READ) == 0, "mprotect");
#endif
    }

private:
    void* data = nullptr;
    std::size_t bytes = 0;
};

template <typename TCall>
static void Rejects(TCall call) {
    errno = 71;
    bool rejected = false;
    try {
        call();
    } catch (const std::runtime_error&) {
        rejected = errno == 71;
    }
    Require(rejected, "faulting guest pointer throws preserving errno");
}

int main() {
    struct { std::int32_t before, bits, after; } output {0x12345678, -777, 0x23456789};
    Require(SSL_CIPHER_get_bits_nid_postfix(nullptr, &output.bits) == 0, "null cipher has zero strength");
    Require(output.bits == -777, "null cipher leaves the optional output untouched");
    Require(SSL_CIPHER_get_bits_nid_postfix(nullptr, nullptr) == 0, "both pointers may be null");
    RequireString(SSL_CIPHER_get_name_nid_postfix(nullptr), "(NONE)");
    RequireString(SSL_CIPHER_get_version_nid_postfix(nullptr), "(NONE)");

    struct { std::uint64_t before; RawCipher cipher; std::uint64_t after; } guarded;
    std::memset(&guarded, 0xa5, sizeof(guarded));
    RawCipher& cipher = guarded.cipher;
    const char customName[] = "arbitrary caller-provided cipher";
    const char* name = customName;
    std::memcpy(cipher.prefix.data() + 8, &name, sizeof(name));
    Require(SSL_CIPHER_get_name_nid_postfix(&cipher) == name, "name pointer is returned without copying or a registry lookup");
    name = nullptr;
    std::memcpy(cipher.prefix.data() + 8, &name, sizeof(name));
    Require(SSL_CIPHER_get_name_nid_postfix(&cipher) == nullptr, "null name field is preserved");
    name = customName;
    std::memcpy(cipher.prefix.data() + 8, &name, sizeof(name));

    constexpr std::int32_t bitPairs[][2] {
        {0, 0}, {40, 128}, {56, 64}, {112, 168}, {128, 128}, {192, 192}, {256, 256},
        {-1, -2}, {std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()},
        {std::numeric_limits<std::int32_t>::max(), std::numeric_limits<std::int32_t>::min()},
    };
    for (const auto& pair : bitPairs) {
        cipher.strength = pair[0];
        cipher.algorithm = pair[1];
        const auto snapshot = guarded;
        output.bits = -777;
        Require(SSL_CIPHER_get_bits_nid_postfix(&cipher, &output.bits) == pair[0], "effective strength is distinct from algorithm bits");
        Require(output.bits == pair[1], "algorithm bits use the 32-bit field at guest offset 84");
        Require(output.before == 0x12345678 && output.after == 0x23456789, "output writes exactly four bytes");
        Require(SSL_CIPHER_get_bits_nid_postfix(&cipher, nullptr) == pair[0], "optional output may be omitted");
        Require(std::memcmp(&guarded, &snapshot, sizeof(guarded)) == 0, "queries preserve the cipher and its guards");
    }

    constexpr std::uint64_t lowIds[] {0, 1, 0x35, 0xc02f, 0xffffff};
    for (std::uint64_t version = 0; version < 256; ++version) {
        for (const auto low : lowIds) {
            const std::uint64_t id = (version << 24) | low;
            std::memcpy(cipher.prefix.data() + 16, &id, sizeof(id));
            RequireString(SSL_CIPHER_get_version_nid_postfix(&cipher), version == 3 ? "TLSv1/SSLv3" : "unknown");
        }
    }
    constexpr std::uint64_t wideIds[] {0x103000035ULL, 0x800000000300c02fULL, 0xffffffff03000000ULL,
                                      std::numeric_limits<std::uint64_t>::max()};
    for (const auto id : wideIds) {
        std::memcpy(cipher.prefix.data() + 16, &id, sizeof(id));
        RequireString(SSL_CIPHER_get_version_nid_postfix(&cipher), "unknown");
    }

    cipher.strength = 40;
    cipher.algorithm = 128;
    Require(SSL_CIPHER_get_bits_nid_postfix(&cipher, &cipher.algorithm) == 40, "output may alias the algorithm field");
    Require(cipher.algorithm == 128, "self-alias preserves algorithm bits");
    Require(SSL_CIPHER_get_bits_nid_postfix(&cipher, &cipher.strength) == 128, "algorithm output is written before strength is read");
    Require(cipher.strength == 128, "aliased output receives algorithm bits");

    const auto snapshot = guarded;
    const std::int32_t valid = 0;
    std::memcpy(cipher.prefix.data(), &valid, sizeof(valid));
    Require(SSL_CIPHER_get_name_nid_postfix(&cipher) == customName, "valid flag does not gate name access");
    Require(SSL_CIPHER_get_bits_nid_postfix(&cipher, nullptr) == 128, "valid flag does not gate strength access");
    Require(guarded.before == snapshot.before && guarded.after == snapshot.after, "input guards remain intact");

    const void* badCipher = reinterpret_cast<const void*>(0x10);
    auto* badBits = reinterpret_cast<std::int32_t*>(0x10);
    std::int32_t bits = 999;
    ReadOnlyPage page;
    std::int32_t* readOnlyBits = page.Set<std::int32_t>(555);
    page.Protect();

    Rejects([&] { SSL_CIPHER_get_bits_nid_postfix(badCipher, &bits); });
    Rejects([&] { SSL_CIPHER_get_bits_nid_postfix(badCipher, nullptr); });
    Rejects([&] { SSL_CIPHER_get_bits_nid_postfix(&cipher, badBits); });
    Rejects([&] { SSL_CIPHER_get_bits_nid_postfix(&cipher, readOnlyBits); });
    Rejects([&] { SSL_CIPHER_get_name_nid_postfix(badCipher); });
    Rejects([&] { SSL_CIPHER_get_version_nid_postfix(badCipher); });
    Require(bits == 999 && *readOnlyBits == 555, "faulting calls leave outputs untouched");
    return 0;
}
