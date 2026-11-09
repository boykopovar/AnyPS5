#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
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
    ApplicationHeapInitialize_nid_no_patch(ApplicationEmptyProcessParameters_nid_no_patch());
    Require(ApplicationHeapDefault_nid_no_patch());
    Guarded memory;
    for (const std::size_t length : {0, 1, 2, 15, 16, 17, 31, 32, 33, 63, 64, 65, 255}) {
        auto* input = memory.data + memory.size - std::max<std::size_t>(length, 1);
        std::fill_n(input, std::max<std::size_t>(length, 1), static_cast<unsigned char>('x'));
        for (const bool terminated : {false, true}) {
            if (terminated && length) input[length / 2] = 0;
            const std::vector<unsigned char> before(input, input + std::max<std::size_t>(length, 1));
            const std::size_t expected = terminated ? length / 2 : length;
            char* result = strndup_nid_postfix(reinterpret_cast<const char*>(input), length);
            Require(result != nullptr && std::strlen(result) == expected);
            Require(std::memcmp(result, input, expected) == 0 && result[expected] == 0);
            free_nid_postfix(result);
            Require(std::equal(before.begin(), before.end(), input));
        }
    }
}
