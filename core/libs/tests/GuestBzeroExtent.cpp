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
        std::fputs("GuestBzeroExtent check failed\n", stderr);
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
    template<class T = unsigned char>
    T* At(std::size_t count) {
        Require(count <= (page - 32) / sizeof(T));
        return reinterpret_cast<T*>(allocation + page * 2 - (std::max)(count, std::size_t{1}) * sizeof(T));
    }
};

template<class T>
static void Prefix(const T* data) {
    for (int index = 1; index <= 8; ++index) Require(data[-index] == static_cast<T>(sizeof(T) == 1 ? 0xa5 : 0xa5a5));
}

int main() {
    Buffer output;
    const auto checkExtent = [&output](std::size_t length) {
        auto* destination = output.At(length);
        const auto extent = (std::max)(length, std::size_t{1});
        std::memset(destination - 16, 0xa5, extent + 16);
        bzero_nid_postfix(destination, length);
        for (std::size_t i = 0; i < length; ++i) Require(destination[i] == 0);
        if (!length) Require(destination[0] == 0xa5);
        Prefix(destination);
    };
    for (std::size_t length = 0; length <= 256; ++length) checkExtent(length);
    for (const std::size_t length : {std::size_t{384}, std::size_t{512}, std::size_t{768}, std::size_t{1024}, std::size_t{1536}, std::size_t{2048}, std::size_t{3072}}) {
        checkExtent(length);
    }

    std::array<unsigned char, 256> region{};
    for (std::size_t offset = 0; offset < 16; ++offset) {
        for (const std::size_t length : {std::size_t{0}, std::size_t{1}, std::size_t{7}, std::size_t{15}, std::size_t{31}, std::size_t{64}, std::size_t{128}}) {
            region.fill(0xa5);
            bzero_nid_postfix(region.data() + offset, length);
            for (std::size_t i = 0; i < region.size(); ++i) {
                const bool zeroed = i >= offset && i < offset + length;
                Require(region[i] == (zeroed ? 0 : 0xa5));
            }
        }
    }
    return 0;
}
