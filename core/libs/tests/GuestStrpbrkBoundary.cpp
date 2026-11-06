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
        std::fputs("GuestStrpbrkBoundary check failed\n", stderr);
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

int main() {
    Buffer input, accept;
    for (std::size_t count = 0; count <= 128; ++count) {
        auto* source = input.At<char>(count + 1);
        for (std::size_t index = 0; index < count; ++index) source[index] = static_cast<char>(1 + (index * 31 + count) % 255);
        source[count] = 0;
        const std::string saved(source, count + 1);
        for (std::size_t size : {0u, 1u, 3u, 16u, 255u}) {
            auto* set = accept.At<char>(size + 1);
            std::array<bool, 256> members{};
            for (std::size_t index = 0; index < size; ++index) {
                set[index] = static_cast<char>(1 + (index * 73 + count) % 255);
                members[static_cast<unsigned char>(set[index])] = true;
            }
            set[size] = 0;
            const std::string setSaved(set, size + 1);
            const char* expected = nullptr;
            for (std::size_t index = 0; index < count; ++index) {
                if (members[static_cast<unsigned char>(source[index])]) { expected = source + index; break; }
            }
            Require(strpbrk_nid_postfix(source, set) == expected);
            Require(std::equal(saved.begin(), saved.end(), source));
            Require(std::equal(setSaved.begin(), setSaved.end(), set));
        }
    }
}
