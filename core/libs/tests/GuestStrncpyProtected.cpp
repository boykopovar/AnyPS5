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
        std::fputs("GuestStrncpyProtected check failed\n", stderr);
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
    for (std::size_t count = 0; count <= 192; ++count) {
        for (std::size_t length : {std::size_t{0}, count / 2, count, count + 1}) {
            const std::string original(length, static_cast<char>(0x81));
            auto* source = reinterpret_cast<char*>(input.At(length + 1));
            auto* destination = reinterpret_cast<char*>(output.At(count));
            std::memcpy(source, original.c_str(), length + 1);
            std::memset(destination - 16, 0xa5, (std::max)(count, std::size_t{1}) + 16);
            Require(strncpy_nid_postfix(destination, source, count) == destination);
            for (std::size_t i = 0; i < count; ++i) Require(destination[i] == (i < length ? static_cast<char>(0x81) : '\0'));
            if (count == 0) Require(static_cast<unsigned char>(*destination) == 0xa5);
            for (int i = 1; i <= 16; ++i) Require(static_cast<unsigned char>(destination[-i]) == 0xa5);
            Require(std::memcmp(source, original.c_str(), length + 1) == 0);
        }
    }
}
