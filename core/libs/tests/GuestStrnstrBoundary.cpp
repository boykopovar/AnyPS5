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
        std::fputs("GuestStrnstrBoundary check failed\n", stderr);
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
    Buffer input, pattern;
    for (std::size_t length = 0; length <= 128; ++length) {
        auto* source = input.At<char>(length);
        const auto extent = (std::max)(length, std::size_t{1});
        for (std::size_t size = 0; size <= 17; ++size) {
            auto* needle = pattern.At<char>(size + 1);
            std::fill(needle, needle + size, 'a');
            if (size) needle[size - 1] = 'b';
            needle[size] = 0;
            const std::string wanted(needle, size);
            for (int mode = 0; mode < 4; ++mode) {
                std::fill(source, source + extent, 'a');
                if (size && size <= length) {
                    const auto position = mode == 0 ? 0 : length - size;
                    std::copy(wanted.begin(), wanted.end(), source + position);
                    if (mode == 2) source[position + size - 1] = 'c';
                }
                if (mode == 3 && length) source[length / 2] = 0;
                const std::string saved(source, extent);
                const auto zero = saved.find(char{0});
                const auto available = (std::min)(length, zero == std::string::npos ? length : zero);
                const auto found = saved.substr(0, available).find(wanted);
                const auto* expected = found == std::string::npos ? nullptr : source + found;
                Require(strnstr_nid_postfix(source, needle, length) == expected);
                Require(std::equal(saved.begin(), saved.end(), source));
                Require(std::equal(wanted.begin(), wanted.end(), needle) && needle[size] == 0);
            }
        }
    }
}
