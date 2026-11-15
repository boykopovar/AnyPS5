#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "memccpy check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(condition) Check((condition), __LINE__)

class GuardedPage {
public:
    std::size_t size;
    unsigned char* data;

    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        data = static_cast<unsigned char*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(data != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(data + size, size, PAGE_NOACCESS, &previous) != 0);
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0);
        size = static_cast<std::size_t>(pageSize);
        data = static_cast<unsigned char*>(mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(data != MAP_FAILED);
        Require(mprotect(data + size, size, PROT_NONE) == 0);
#endif
    }

    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, size * 2);
#endif
    }
};

int main() {
    GuardedPage source;
    GuardedPage destination;
    for (std::size_t count = 0; count <= 256; ++count) {
        for (const std::size_t stop : {count, count / 2, std::size_t{0}}) {
            std::memset(source.data, 0x35, source.size);
            std::memset(destination.data, 0xA7, destination.size);
            auto* input = source.data + source.size - std::max(count, std::size_t{1});
            auto* output = destination.data + destination.size - std::max(count, std::size_t{1});
            if (stop < count) input[stop] = 0xFF;
            const std::vector<unsigned char> before(source.data, source.data + source.size);
            std::vector<unsigned char> expected(destination.data, destination.data + destination.size);
            const std::size_t copied = stop < count ? stop + 1 : count;
            std::copy_n(input, copied, expected.data() + (output - destination.data));
            void* result = memccpy_nid_postfix(output, input, 0x1FF, count);
            Require(result == (stop < count ? output + copied : nullptr));
            Require(std::memcmp(destination.data, expected.data(), destination.size) == 0);
            Require(std::memcmp(source.data, before.data(), source.size) == 0);
        }
    }
    for (const int character : {-1, 0xFF, 0x1FF}) {
        source.data[source.size - 1] = 0xFF;
        destination.data[destination.size - 1] = 0;
        Require(memccpy_nid_postfix(destination.data + destination.size - 1, source.data + source.size - 1,
                                  character, 256) == destination.data + destination.size);
        Require(destination.data[destination.size - 1] == 0xFF);
    }
    std::puts("memccpy return values, stop bytes, guards and unsigned conversion passed");
}
