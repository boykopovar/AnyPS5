#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <source_location>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %u\n", location.line());
        std::abort();
    }
}

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

class Guarded {
public:
    unsigned char* data = nullptr;
    std::size_t size = 0;
    Guarded() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        data = static_cast<unsigned char*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(data != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(data + size, size, PAGE_NOACCESS, &previous) != 0);
#else
        const auto pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0);
        size = static_cast<std::size_t>(pageSize);
        auto* mapping = mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapping != MAP_FAILED);
        data = static_cast<unsigned char*>(mapping);
        Require(mprotect(data + size, size, PROT_NONE) == 0);
#endif
    }
    ~Guarded() {
#ifdef _WIN32
        Require(VirtualFree(data, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(data, size * 2) == 0);
#endif
    }
    Guarded(const Guarded&) = delete;
    Guarded& operator=(const Guarded&) = delete;
};

int main() {
    Guarded memory;
    for (const std::size_t length : {0, 1, 2, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129, 255}) {
        auto* input = memory.data + memory.size - std::max<std::size_t>(length, 1);
        std::fill_n(input, std::max<std::size_t>(length, 1), static_cast<unsigned char>(0x81));
        for (const bool matches : {false, true}) {
            if (matches && length) { input[0] = 0xfe; input[length - 1] = 0xfe; }
            const std::vector<unsigned char> before(input, input + std::max<std::size_t>(length, 1));
            const void* first = nullptr;
            const void* last = nullptr;
            for (std::size_t i = 0; i < length; ++i) {
                if (input[i] == 0xfe) { if (!first) first = input + i; last = input + i; }
            }
            Require(memchr_nid_postfix(input, 0xfe, length) == first);
            Require(memrchr_nid_postfix(input, 0xfe, length) == last);
            Require(strnlen_nid_postfix(reinterpret_cast<const char*>(input), length) == length);
            Require(std::equal(before.begin(), before.end(), input));
        }
        if (length) {
            input[length - 1] = 0;
            Require(strnlen_nid_postfix(reinterpret_cast<const char*>(input), length) == length - 1);
            input[0] = 0;
            Require(strnlen_nid_postfix(reinterpret_cast<const char*>(input), length) == 0);
        }
    }
}
