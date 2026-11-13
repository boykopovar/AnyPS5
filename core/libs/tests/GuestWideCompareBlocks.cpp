#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Require(bool condition) {
    if (!condition) {
        std::fputs("GuestWideCompareBlocks check failed\n", stderr);
        std::abort();
    }
}

class Page {
    unsigned char* allocation;
    std::size_t page;
public:
    Page() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
        allocation = static_cast<unsigned char*>(VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(allocation != nullptr);
        DWORD previous;
        Require(VirtualProtect(allocation + page, page, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        page = static_cast<std::size_t>(size);
        allocation = static_cast<unsigned char*>(mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(allocation != MAP_FAILED);
        Require(mprotect(allocation + page, page, PROT_NONE) == 0);
#endif
    }
    ~Page() {
#ifdef _WIN32
        VirtualFree(allocation, 0, MEM_RELEASE);
#else
        munmap(allocation, page * 2);
#endif
    }
    Page(const Page&) = delete;
    Page& operator=(const Page&) = delete;
    char16_t* End(std::size_t units, std::size_t gap) {
        return reinterpret_cast<char16_t*>(allocation + page - gap - units * sizeof(char16_t));
    }
};

static int Sign(int value) {
    return (value > 0) - (value < 0);
}

static int ReferenceCompare(const char16_t* s1, const char16_t* s2, std::size_t n, bool stopAtZero) {
    for (; n != 0; ++s1, ++s2, --n) {
        if (*s1 != *s2) return *s1 < *s2 ? -1 : 1;
        if (stopAtZero && *s1 == 0) return 0;
    }
    return 0;
}

int main() {
    std::mt19937 random(0xc0de);
    Page first, second;
    const char16_t alphabet[] = {1, 0x41, 0xff, 0x100, 0x7fff, 0x8000, 0xd800, 0xffff};
    for (int round = 0; round < 40000; ++round) {
        const std::size_t length = random() % 3 == 0 ? random() % 24 : random() % 600;
        const std::size_t gap1 = random() % 2 == 0 ? 0 : random() % 64 * 2;
        const std::size_t gap2 = random() % 2 == 0 ? 0 : random() % 64 * 2;
        char16_t* left = first.End(length + 1, gap1);
        char16_t* right = second.End(length + 1, gap2);
        for (std::size_t index = 0; index < length; ++index) left[index] = alphabet[random() % 8];
        left[length] = 0;
        std::memcpy(right, left, (length + 1) * sizeof(char16_t));
        const int changes = random() % 3;
        for (int change = 0; change < changes; ++change) {
            right[random() % (length + 1)] = random() % 5 == 0 ? char16_t{0} : alphabet[random() % 8];
        }
        Require(Sign(wcscmp_nid_postfix(left, right)) == ReferenceCompare(left, right, SIZE_MAX, true));
        Require(Sign(wcscmp_nid_postfix(right, left)) == ReferenceCompare(right, left, SIZE_MAX, true));
        const std::size_t limit = random() % 2 == 0 ? random() % (length + 2) : SIZE_MAX;
        Require(Sign(wcsncmp_nid_postfix(left, right, limit)) == ReferenceCompare(left, right, limit, true));
        const std::size_t extent = random() % (length + 2);
        Require(Sign(wmemcmp_nid_postfix(left, right, extent)) == ReferenceCompare(left, right, extent, false));
        Require(Sign(wmemcmp_nid_postfix(right, left, extent)) == ReferenceCompare(right, left, extent, false));
    }
}
