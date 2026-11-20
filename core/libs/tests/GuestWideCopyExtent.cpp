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
        std::fputs("GuestWideCopyExtent check failed\n", stderr);
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
    for (std::size_t count = 0; count <= 128; ++count) {
        auto* source = input.At<char16_t>(count + 1);
        for (std::size_t index = 0; index < count; ++index) source[index] = static_cast<char16_t>(0x100 + index * 257);
        source[count] = 0;
        const std::u16string saved(source, count + 1);
        auto* copied = output.At<char16_t>(count + 1);
        std::memset(copied - 8, 0xa5, (count + 9) * sizeof(char16_t));
        Require(wcscpy_nid_postfix(copied, source) == copied);
        Require(std::equal(saved.begin(), saved.end(), copied));
        Prefix(copied);
        for (std::size_t prefix : {0u, 1u, 15u, 33u}) {
            auto* destination = output.At<char16_t>(prefix + count + 1);
            std::memset(destination - 8, 0xa5, (prefix + count + 9) * sizeof(char16_t));
            const std::u16string start(prefix, 0xd800);
            std::copy(start.begin(), start.end(), destination);
            destination[prefix] = 0;
            const auto expected = start + saved;
            Require(wcscat_nid_postfix(destination, source) == destination);
            Require(std::equal(expected.begin(), expected.end(), destination));
            Require(std::equal(saved.begin(), saved.end(), source));
            Prefix(destination);
        }
    }
}
