#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static void Require(bool condition) {
    if (!condition) {
        std::fputs("GuestMemcmpProtected check failed\n", stderr);
        std::abort();
    }
}

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

class Buffer {
    unsigned char* allocation;
    std::size_t page;
public:
    Buffer() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
        allocation = static_cast<unsigned char*>(VirtualAlloc(nullptr, page * 3, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
        Require(allocation != nullptr);
        DWORD previous;
        Require(VirtualProtect(allocation + page, page, PAGE_READWRITE, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        page = static_cast<std::size_t>(size);
        allocation = static_cast<unsigned char*>(mmap(nullptr, page * 3, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(allocation != MAP_FAILED);
        Require(mprotect(allocation + page, page, PROT_READ | PROT_WRITE) == 0);
#endif
        std::memset(allocation + page, 0xa5, page);
    }
    ~Buffer() {
#ifdef _WIN32
        Require(VirtualFree(allocation, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(allocation, page * 3) == 0);
#endif
    }
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    unsigned char* At(std::size_t count) {
        Require(count <= page - 32);
        return allocation + page * 2 - (std::max)(count, std::size_t{1});
    }
};

static int Sign(int value) { return (value > 0) - (value < 0); }

int main() {
    Buffer left, right;
    auto* a = left.At(1);
    auto* b = right.At(1);
    for (unsigned x = 0; x < 256; ++x) {
        for (unsigned y = 0; y < 256; ++y) {
            *a = static_cast<unsigned char>(x);
            *b = static_cast<unsigned char>(y);
            Require(Sign(memcmp_nid_postfix(a, b, 1)) == Sign(static_cast<int>(x) - static_cast<int>(y)));
            Require(*a == x && *b == y);
        }
    }
    for (std::size_t length = 0; length <= 128; ++length) {
        a = left.At(length);
        b = right.At(length);
        const auto extent = (std::max)(length, std::size_t{1});
        std::memset(a, 0x80, extent);
        std::memset(b, 0x80, extent);
        Require(memcmp_nid_postfix(a, b, length) == 0);
        for (std::size_t position = 0; position < length; ++position) {
            a[position] = 0xff;
            b[position] = 0x7f;
            if (position + 1 < length) { a[position + 1] = 0; b[position + 1] = 0xff; }
            Require(memcmp_nid_postfix(a, b, length) > 0);
            Require(memcmp_nid_postfix(b, a, length) < 0);
            Require(memcmp_nid_postfix(a, b, position) == 0);
            std::memset(a, 0x80, extent);
            std::memset(b, 0x80, extent);
        }
    }
}
