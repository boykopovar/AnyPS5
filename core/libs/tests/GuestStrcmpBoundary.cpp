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
        std::fputs("GuestStrcmpBoundary check failed\n", stderr);
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

static int Sign(int value) {
    return (value > 0) - (value < 0);
}

int main() {
    Buffer first, second;
    for (std::size_t count = 0; count <= 128; ++count) {
        auto* left = first.At(count + 1);
        auto* right = second.At(count + 1);
        for (std::size_t index = 0; index < count; ++index) left[index] = static_cast<unsigned char>(1 + (index * 73 + count) % 255);
        left[count] = 0;
        const std::vector<unsigned char> saved(left, left + count + 1);
        for (std::size_t position = 0; position <= count; ++position) {
            for (unsigned char value : {0, 1, 127, 128, 255}) {
                if (position == count && value != 0) continue;
                std::copy(saved.begin(), saved.end(), right);
                right[position] = value;
                int expected = 0;
                for (std::size_t index = 0; index <= count; ++index) {
                    if (left[index] != right[index]) { expected = static_cast<int>(left[index]) - right[index]; break; }
                    if (left[index] == 0) break;
                }
                const std::vector<unsigned char> other(right, right + count + 1);
                Require(Sign(strcmp_nid_postfix(reinterpret_cast<char*>(left), reinterpret_cast<char*>(right))) == Sign(expected));
                Require(std::equal(saved.begin(), saved.end(), left));
                Require(std::equal(other.begin(), other.end(), right));
            }
        }
    }
}
