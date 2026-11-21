#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {
thread_local bool armed = false, fired = false;
thread_local std::size_t failAt = 0, allocations = 0;
thread_local std::array<void*, 256> tracked{};
std::size_t failures = 0, checks = 0, faultCases = 0, escaped = 0;
void Check(bool value, const char* label) {
    ++checks;
    if (value) return;
    ++failures;
    if (failures <= 8) std::fprintf(stderr, "FAIL: %s\n", label);
}
std::size_t Live() {
    return std::count_if(tracked.begin(), tracked.end(), [](void* p) { return p != nullptr; });
}

struct TestCipher {
    std::int32_t valid;
    const char* name;
    std::uint64_t id;
    std::uint64_t keyExchange;
    std::uint64_t authentication;
    std::uint64_t encryption;
    std::uint64_t mac;
    std::uint64_t protocol;
    std::uint64_t strengthFlags;
    std::uint64_t extraFlags;
    std::int32_t strengthBits;
    std::int32_t algorithmBits;
};

static_assert(sizeof(TestCipher) == 88);
static_assert(offsetof(TestCipher, strengthBits) == 80);
static_assert(offsetof(TestCipher, algorithmBits) == 84);

struct StraddlingCipherPages {
    unsigned char* base = nullptr;
    std::size_t pageSize = 0;

    StraddlingCipherPages() {
#if defined(_WIN32)
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Check(base != nullptr, "VirtualAlloc succeeds for SSL_CIPHER_get_bits page test");
        if (!base) return;
        DWORD previous = 0;
        const BOOL protectedOk = VirtualProtect(base, pageSize, PAGE_NOACCESS, &previous);
        Check(protectedOk != 0, "VirtualProtect makes first page inaccessible");
        if (!protectedOk) {
            VirtualFree(base, 0, MEM_RELEASE);
            base = nullptr;
        }
#else
        const long systemPageSize = sysconf(_SC_PAGESIZE);
        Check(systemPageSize > 0, "sysconf returns positive page size");
        if (systemPageSize <= 0) return;
        pageSize = static_cast<std::size_t>(systemPageSize);
        void* mapped = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Check(mapped != MAP_FAILED, "mmap succeeds for SSL_CIPHER_get_bits page test");
        if (mapped == MAP_FAILED) return;
        base = static_cast<unsigned char*>(mapped);
        const int protectedOk = mprotect(base, pageSize, PROT_NONE);
        Check(protectedOk == 0, "mprotect makes first page inaccessible");
        if (protectedOk != 0) {
            munmap(base, pageSize * 2);
            base = nullptr;
        }
#endif
    }

    ~StraddlingCipherPages() {
        if (!base) return;
#if defined(_WIN32)
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, pageSize * 2);
#endif
    }

    unsigned char* ReadablePage() const {
        return base ? base + pageSize : nullptr;
    }
};

void CipherGetBitsFault() {
    std::int32_t algorithmBits = 0x12345678;
    Check(SSL_CIPHER_get_bits_nid_postfix(nullptr, &algorithmBits) == 0 && algorithmBits == 0x12345678,
          "null cipher returns zero without modifying algorithmBits");

    TestCipher validCipher{};
    validCipher.strengthBits = 128;
    validCipher.algorithmBits = 256;
    algorithmBits = 0;
    Check(SSL_CIPHER_get_bits_nid_postfix(&validCipher, &algorithmBits) == 128 && algorithmBits == 256,
          "valid cipher returns strengthBits and writes algorithmBits");
    Check(SSL_CIPHER_get_bits_nid_postfix(&validCipher, nullptr) == 128,
          "valid cipher with null algorithmBits returns strengthBits");

    StraddlingCipherPages pages;
    unsigned char* readablePage = pages.ReadablePage();
    if (!readablePage) return;

    constexpr std::int32_t expectedAlgorithmBits = 256;
    std::memcpy(readablePage, &expectedAlgorithmBits, sizeof(expectedAlgorithmBits));
    const void* partialCipher = readablePage - offsetof(TestCipher, algorithmBits);

    algorithmBits = 0x12345678;
    bool threw = false;
    try {
        SSL_CIPHER_get_bits_nid_postfix(partialCipher, &algorithmBits);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Check(threw, "partial cipher across page boundary throws");
    Check(algorithmBits == 0x12345678, "partial cipher fault leaves algorithmBits unchanged");
}
}

void* operator new(std::size_t size) {
    if (armed) {
        ++allocations;
        if (failAt == allocations) { fired = true; throw std::bad_alloc(); }
    }
    void* pointer = std::malloc(size ? size : 1);
    if (!pointer) throw std::bad_alloc();
    if (armed) {
        auto slot = std::find(tracked.begin(), tracked.end(), nullptr);
        if (slot == tracked.end()) std::abort();
        *slot = pointer;
    }
    return pointer;
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept {
    if (pointer) {
        auto slot = std::find(tracked.begin(), tracked.end(), pointer);
        if (slot != tracked.end()) *slot = nullptr;
    }
    std::free(pointer);
}
void operator delete[](void* pointer) noexcept { ::operator delete(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { ::operator delete(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { ::operator delete(pointer); }

struct NumericRun {
    std::array<char, 512> buffer;
    int returned;
    bool threw, allocationFailed;
    std::size_t calls, live;
    std::uint64_t error, following;
};
NumericRun Run(const unsigned char* data, int length, int space, std::size_t fail, bool seed = false) {
    NumericRun result{};
    result.buffer.fill('Z');
    LibreSslAsn1Object object{nullptr, nullptr, 0, length, data, 0};
    ERR_clear_error_nid_postfix();
    if (seed) LibreSslPutError_nid_no_patch(8, 0xfff, 101);
    allocations = 0; failAt = fail; fired = false;
    armed = true;
    try { result.returned = i2t_ASN1_OBJECT_nid_postfix(result.buffer.data(), space, &object); }
    catch (const std::bad_alloc&) { result.threw = true; }
    armed = false;
    result.calls = allocations;
    result.allocationFailed = fired;
    result.live = Live();
    result.error = ERR_get_error_nid_postfix();
    result.following = ERR_get_error_nid_postfix();
    return result;
}

std::string Name(const std::vector<unsigned char>& data) {
    LibreSslAsn1Object object{nullptr, nullptr, 0, static_cast<int>(data.size()), data.data(), 0};
    char buffer[64];
    i2t_ASN1_OBJECT_nid_postfix(buffer, sizeof(buffer), &object);
    return buffer;
}

void CreateFailures() {
    const std::vector<unsigned char> replaced{0x2a, 0x03, 0x04}, fresh{0x2a, 0x03, 0x05};
    for (std::size_t point = 1;; ++point) {
        OBJ_cleanup_nid_postfix();
        Check(OBJ_create_nid_postfix("1.2.3.4", "base", nullptr) > 0, "the object to replace is added");
        allocations = 0; failAt = point; fired = false;
        armed = true;
        bool threw = false;
        int replacing = 0;
        try { replacing = OBJ_create_nid_postfix("1.2.3.4", "replacing", nullptr); }
        catch (const std::bad_alloc&) { threw = true; }
        armed = false;
        const bool failed = fired;
        ++faultCases;
        Check(threw == failed, "an allocation failure in OBJ_create throws std::bad_alloc");
        Check(Name(replaced) == (failed ? "base" : "replacing"), "a failed OBJ_create leaves the replaced encoding as it was");
        const int next = OBJ_create_nid_postfix("1.2.3.5", "fresh", nullptr);
        Check(next > 0 && (failed || next == replacing + 1), "objects can still be added after a failed OBJ_create");
        Check(Name(fresh) == "fresh", "lookups still work after a failed OBJ_create");
        OBJ_cleanup_nid_postfix();
        Check(Live() == 0, "OBJ_cleanup frees everything a failed OBJ_create allocated");
        Check(Name(replaced) == "1.2.3.4" && Name(fresh) == "1.2.3.5", "OBJ_cleanup removes every added object");
        if (!failed) break;
    }
}

int main() {
    constexpr std::uint64_t allocationError = (3ull << 24) | (0xfffull << 12) | 65;
    std::size_t fastCases = 0;
    const unsigned char warmUp[] {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f};
    Run(warmUp, sizeof(warmUp), 0, 1);
    tracked.fill(nullptr);
    const unsigned char shortOid[] {0x2a, 0x03, 0x04};
    const unsigned char maxWord[] {0x81,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f};
    const unsigned char malformed[] {0x2a,0x81};
    for (const auto bytes : {std::pair{shortOid, sizeof(shortOid)}, std::pair{maxWord, sizeof(maxWord)}, std::pair{malformed, sizeof(malformed)}}) {
        for (int space : {0, 1, 2, 5, 32, 512}) {
            const auto expected = Run(bytes.first, static_cast<int>(bytes.second), space, 0);
            const auto result = Run(bytes.first, static_cast<int>(bytes.second), space, 1);
            ++fastCases;
            Check(!result.threw && result.returned == expected.returned && result.buffer == expected.buffer, "word/malformed path works when host new would fail");
            Check(result.calls == 0 && !result.allocationFailed, "word/malformed path does not allocate");
            Check(result.live == 0 && result.error == 0, "word/malformed path preserves empty error queue");
            escaped += result.threw;
        }
    }
    for (int size : {11, 20, 40}) {
        for (bool later : {false, true}) {
            std::vector<unsigned char> bytes;
            if (later) bytes = {0x2a, 0x03};
            bytes.insert(bytes.end(), static_cast<std::size_t>(size), 0xff);
            bytes.back() = 0x7f;
            for (int space : {0, 1, 5, 512}) {
                const auto normal = Run(bytes.data(), static_cast<int>(bytes.size()), space, 0);
                for (std::size_t point = 1; point <= normal.calls; ++point) {
                    const auto result = Run(bytes.data(), static_cast<int>(bytes.size()), space, point);
                    ++faultCases;
                    escaped += result.threw;
                    Check(result.allocationFailed, "selected big-number allocation actually failed");
                    Check(!result.threw && result.returned == 0, "big-number allocation failure returns zero");
                    Check(result.error == allocationError && result.following == 0, "big-number allocation failure uses BN malloc error");
                    Check(result.live == 0, "failed conversion releases scratch allocations");
                    if (space > 0) {
                        const void* end = std::memchr(result.buffer.data(), 0, static_cast<std::size_t>(space));
                        Check(end != nullptr, "failed conversion leaves output terminated");
                    }
                }
            }
        }
    }
    const unsigned char big[] {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f};
    const auto seeded = Run(big, sizeof(big), 64, 1, true);
    escaped += seeded.threw;
    Check(!seeded.threw && seeded.returned == 0 && seeded.error == ((8ull << 24) | (0xfffull << 12) | 101) && seeded.following == allocationError,
          "allocation error appends to existing queue entries");
    Check(ERR_get_error_nid_postfix() == 0 && seeded.live == 0, "seeded failure adds exactly one error and leaks nothing");
    CreateFailures();
    CipherGetBitsFault();
    std::printf("{\"fast_cases\":%zu,\"allocation_fault_cases\":%zu,\"checks\":%zu,\"failures\":%zu,\"escaped\":%zu,\"final_live\":%zu}\n",
                fastCases, faultCases, checks, failures, escaped, Live());
    return failures ? 1 : 0;
}
