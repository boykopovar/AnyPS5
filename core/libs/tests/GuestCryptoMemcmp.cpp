#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool value) {
    if (!value) {
        std::fprintf(stderr, "CRYPTO_memcmp check failed\n");
        std::abort();
    }
}

class GuardedPage {
public:
    explicit GuardedPage(bool different) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(memory != nullptr);
        DWORD previous;
        Require(VirtualProtect(memory + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long nativePageSize = sysconf(_SC_PAGESIZE);
        Require(nativePageSize > 0);
        pageSize = static_cast<std::size_t>(nativePageSize);
        void* allocation = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(allocation != MAP_FAILED);
        memory = static_cast<unsigned char*>(allocation);
        Require(mprotect(memory + pageSize, pageSize, PROT_NONE) == 0);
#endif
        std::memset(memory, 0xA5, pageSize);
        if (different) memory[pageSize - 1] ^= 0xFF;
#ifdef _WIN32
        Require(VirtualProtect(memory, pageSize, PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(memory, pageSize, PROT_READ) == 0);
#endif
    }

    GuardedPage(const GuardedPage&) = delete;
    GuardedPage& operator=(const GuardedPage&) = delete;

    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, pageSize * 2);
#endif
    }

    const unsigned char* Tail(std::size_t length) const {
        Require(length <= pageSize);
        return memory + pageSize - length;
    }

private:
    unsigned char* memory = nullptr;
    std::size_t pageSize = 0;
};

}

int main() {
    Require(CRYPTO_memcmp_nid_postfix(nullptr, nullptr, 0) == 0);
    unsigned char byte = 0xA5;
    Require(CRYPTO_memcmp_nid_postfix(nullptr, &byte, 0) == 0);
    Require(CRYPTO_memcmp_nid_postfix(&byte, nullptr, 0) == 0);
    for (unsigned int left = 0; left < 256; ++left) {
        for (unsigned int right = 0; right < 256; ++right) {
            const unsigned char a = static_cast<unsigned char>(left);
            const unsigned char b = static_cast<unsigned char>(right);
            Require((CRYPTO_memcmp_nid_postfix(&a, &b, 1) == 0) == (left == right));
        }
    }
    std::array<unsigned char, 67> first;
    for (std::size_t index = 0; index < first.size(); ++index) first[index] = static_cast<unsigned char>(index * 37);
    auto second = first;
    const auto original = first;
    Require(CRYPTO_memcmp_nid_postfix(first.data(), first.data(), first.size()) == 0);
    Require(CRYPTO_memcmp_nid_postfix(first.data() + 1, second.data() + 1, 65) == 0);
    for (std::size_t index = 1; index <= 65; ++index) {
        second[index] ^= 0x80;
        const auto changed = second;
        Require(CRYPTO_memcmp_nid_postfix(first.data() + 1, second.data() + 1, 65) != 0);
        Require(CRYPTO_memcmp_nid_postfix(second.data() + 1, first.data() + 1, 65) != 0);
        Require(first == original && second == changed);
        second[index] ^= 0x80;
    }
    second[32] ^= 0x80;
    Require(CRYPTO_memcmp_nid_postfix(first.data(), second.data(), 32) == 0);
    Require(CRYPTO_memcmp_nid_postfix(first.data(), second.data(), 33) != 0);
    second = first;
    second[1] ^= 0x80;
    second[65] ^= 0x80;
    Require(CRYPTO_memcmp_nid_postfix(first.data(), second.data(), first.size()) != 0);
    first.fill(0xCC);
    Require(CRYPTO_memcmp_nid_postfix(first.data(), first.data() + 1, first.size() - 1) == 0);
    first[32] = 0xCD;
    const auto overlap = first;
    Require(CRYPTO_memcmp_nid_postfix(first.data(), first.data() + 1, first.size() - 1) != 0);
    Require(first == overlap);
    const GuardedPage equal(false);
    const GuardedPage other(false);
    const GuardedPage different(true);
    for (const std::size_t length : {0, 1, 2, 7, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129}) {
        Require(CRYPTO_memcmp_nid_postfix(equal.Tail(length), other.Tail(length), length) == 0);
        Require((CRYPTO_memcmp_nid_postfix(equal.Tail(length), different.Tail(length), length) == 0) == (length == 0));
    }
}
