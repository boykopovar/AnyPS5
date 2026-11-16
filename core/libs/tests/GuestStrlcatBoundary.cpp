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
        std::fputs("GuestStrlcatBoundary check failed\n", stderr);
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
    Buffer input, output;
    for (std::size_t capacity = 0; capacity <= 128; ++capacity) {
        for (std::size_t prefix : {std::size_t{0}, capacity / 2, capacity == 0 ? 0 : capacity - 1, capacity}) {
            for (std::size_t size : {0u, 1u, 15u, 128u}) {
                auto* source = input.At<char>(size + 1);
                for (std::size_t index = 0; index < size; ++index) source[index] = static_cast<char>(1 + (index * 73 + size) % 255);
                source[size] = 0;
                const std::string saved(source, size + 1);
                auto* destination = output.At<char>(capacity);
                const auto extent = (std::max)(capacity, std::size_t{1});
                std::memset(destination - 16, 0xa5, extent + 16);
                std::fill(destination, destination + prefix, 'p');
                if (prefix < capacity) destination[prefix] = 0;
                std::vector<char> expected(destination, destination + extent);
                if (prefix < capacity) {
                    const auto copied = (std::min)(size, capacity - prefix - 1);
                    std::copy(saved.begin(), saved.begin() + copied, expected.begin() + prefix);
                    expected[prefix + copied] = 0;
                }
                Require(strlcat_nid_postfix(destination, source, capacity) == prefix + size);
                Require(std::equal(expected.begin(), expected.end(), destination));
                Require(std::equal(saved.begin(), saved.end(), source));
                Prefix(reinterpret_cast<unsigned char*>(destination));
            }
        }
    }
}
