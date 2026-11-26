#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Require(bool condition) {
    if (!condition) {
        std::fputs("GuestMemrchrBlocks check failed\n", stderr);
        std::abort();
    }
}

class GuardedPage {
    unsigned char* allocation;
public:
    unsigned char* data;
    std::size_t size;
    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        allocation = static_cast<unsigned char*>(VirtualAlloc(nullptr, size * 3, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
        Require(allocation != nullptr);
        DWORD previous;
        Require(VirtualProtect(allocation + size, size, PAGE_READWRITE, &previous) != 0);
#else
        const long page = sysconf(_SC_PAGESIZE);
        Require(page > 0);
        size = static_cast<std::size_t>(page);
        allocation = static_cast<unsigned char*>(mmap(nullptr, size * 3, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(allocation != MAP_FAILED);
        Require(mprotect(allocation + size, size, PROT_READ | PROT_WRITE) == 0);
#endif
        data = allocation + size;
    }
    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(allocation, 0, MEM_RELEASE);
#else
        munmap(allocation, size * 3);
#endif
    }
    GuardedPage(const GuardedPage&) = delete;
    GuardedPage& operator=(const GuardedPage&) = delete;
};

static const unsigned char* ReferenceLast(const unsigned char* s, unsigned char value, std::size_t n) {
    while (n != 0) {
        --n;
        if (s[n] == value) return s + n;
    }
    return nullptr;
}

int main() {
    std::mt19937 random(0xb17e);
    GuardedPage source;
    GuardedPage destination;
    for (int round = 0; round < 40000; ++round) {
        for (std::size_t index = 0; index < source.size; ++index) source.data[index] = static_cast<unsigned char>(random() % 4 + 0xfd);
        const std::size_t length = random() % 3 == 0 ? random() % 40 : random() % 700;
        const std::size_t room = source.size - length;
        const std::size_t offset = random() % 3 == 0 ? random() % (std::min)(room + 1, std::size_t{48})
            : random() % 3 == 0 ? room - random() % (std::min)(room + 1, std::size_t{48}) : random() % (room + 1);
        const unsigned char* input = source.data + offset;
        const int value = random() % 5 == 0 ? 0x1fd : static_cast<int>(random() % 4 + 0xfd);
        Require(memrchr_nid_postfix(input, value, length) == ReferenceLast(input, static_cast<unsigned char>(value), length));

        const std::size_t outputOffset = random() % (destination.size - length + 1);
        unsigned char* output = destination.data + outputOffset;
        std::memset(destination.data, 0x5a, destination.size);
        std::vector<unsigned char> expected(destination.data, destination.data + destination.size);
        const auto* found = static_cast<const unsigned char*>(std::memchr(input, static_cast<unsigned char>(value), length));
        const std::size_t copied = found == nullptr ? length : static_cast<std::size_t>(found - input) + 1;
        std::memcpy(expected.data() + outputOffset, input, copied);
        void* result = memccpy_nid_postfix(output, input, value, length);
        Require(result == (found == nullptr ? nullptr : output + copied));
        Require(std::memcmp(destination.data, expected.data(), destination.size) == 0);
    }
    Require(memrchr_nid_postfix(nullptr, 0, 0) == nullptr);
    Require(memccpy_nid_postfix(nullptr, nullptr, 0, 0) == nullptr);
}
