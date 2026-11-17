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
        std::fputs("GuestWideLengthProtected check failed\n", stderr);
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

int main() {
    Buffer input;
    constexpr std::array<char16_t, 5> values = {0x100, 0xff00, 0xd800, 0xdc00, 0xffff};
    for (std::size_t limit = 0; limit <= 256; ++limit) {
        const auto extent = (std::max)(limit, std::size_t{1});
        auto* source = reinterpret_cast<char16_t*>(input.At(extent * sizeof(char16_t)));
        for (std::size_t stop : {limit, std::size_t{0}, limit / 2}) {
            for (std::size_t i = 0; i < extent; ++i) source[i] = values[i % values.size()];
            if (stop < limit) source[stop] = 0;
            const std::vector<char16_t> before(source, source + extent);
            Require(wcsnlen_s_nid_postfix(source, limit) == stop);
            Require(std::equal(before.begin(), before.end(), source));
        }
    }
}
