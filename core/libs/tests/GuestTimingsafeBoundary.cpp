#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void Require(bool condition) {
    if (!condition) {
        std::fputs("GuestTimingsafeBoundary check failed\n", stderr);
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

static int Sign(int value) {
    return (value > 0) - (value < 0);
}

int main() {
    Buffer first, second;
    for (std::size_t length = 0; length <= 128; ++length) {
        auto* left = first.At(length);
        auto* right = second.At(length);
        const auto extent = (std::max)(length, std::size_t{1});
        for (int mode = 0; mode < 4; ++mode) {
            for (std::size_t i = 0; i < extent; ++i) left[i] = right[i] = static_cast<unsigned char>(i * 131 + length);
            if (length && mode < 3) {
                const auto position = mode == 0 ? 0 : mode == 1 ? length / 2 : length - 1;
                left[position] = 0xff; right[position] = 1;
                if (position + 1 < length) { left[position + 1] = 1; right[position + 1] = 0xff; }
            }
            int expected = 0;
            for (std::size_t i = 0; i < length; ++i) if (left[i] != right[i]) { expected = left[i] > right[i] ? 1 : -1; break; }
            const std::vector<unsigned char> a(left, left + extent), b(right, right + extent);
            Require(Sign(timingsafe_memcmp_nid_postfix(left, right, length)) == expected);
            Require(Sign(timingsafe_memcmp_nid_postfix(right, left, length)) == -expected);
            Require(std::equal(a.begin(), a.end(), left) && std::equal(b.begin(), b.end(), right));
        }
    }
}
