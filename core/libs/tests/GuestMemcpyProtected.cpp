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
        std::fputs("GuestMemcpyProtected check failed\n", stderr);
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
    Buffer input, output;
    for (std::size_t length = 0; length <= 256; ++length) {
        auto* source = input.At(length);
        auto* destination = output.At(length);
        const auto extent = (std::max)(length, std::size_t{1});
        for (std::size_t i = 0; i < extent; ++i) source[i] = static_cast<unsigned char>(i * 131 + length);
        std::vector<unsigned char> before(source, source + extent);
        std::memset(destination - 16, 0xa5, extent + 16);
        Require(memcpy_nid_postfix(destination, source, length) == destination);
        for (std::size_t i = 0; i < length; ++i) Require(destination[i] == before[i]);
        if (length == 0) Require(destination[0] == 0xa5);
        Require(std::equal(before.begin(), before.end(), source));
        for (int i = 1; i <= 16; ++i) Require(destination[-i] == 0xa5);
    }
}
