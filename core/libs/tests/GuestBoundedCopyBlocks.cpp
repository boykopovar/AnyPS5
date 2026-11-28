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
        std::fputs("GuestBoundedCopyBlocks check failed\n", stderr);
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

static std::size_t ReferenceLength(const char* s, std::size_t limit) {
    std::size_t length = 0;
    while (length < limit && s[length] != '\0') ++length;
    return length;
}

int main() {
    std::mt19937 random(0x57c9);
    GuardedPage source;
    GuardedPage destination;
    for (int round = 0; round < 40000; ++round) {
        for (std::size_t index = 0; index < source.size; ++index) {
            source.data[index] = static_cast<unsigned char>(random() % 8 == 0 ? 0 : 'a' + random() % 26);
        }
        const std::size_t room = random() % 3 == 0 ? random() % 48 : random() % 900;
        const std::size_t offset = random() % 2 == 0 ? source.size - room : random() % (source.size - room + 1);
        const char* input = reinterpret_cast<const char*>(source.data + offset);
        if (random() % 3 == 0) std::memset(source.data + offset, 'x', room);
        const std::size_t limit = random() % (room + 1);
        Require(strnlen_nid_postfix(input, limit) == ReferenceLength(input, limit));
        Require(strnlen_s_nid_postfix(input, limit) == ReferenceLength(input, limit));

        const std::size_t outputOffset = random() % (destination.size - limit + 1);
        char* output = reinterpret_cast<char*>(destination.data + outputOffset);
        std::memset(destination.data, 0x5a, destination.size);
        std::vector<unsigned char> expected(destination.data, destination.data + destination.size);
        const std::size_t length = ReferenceLength(input, limit);
        std::memcpy(expected.data() + outputOffset, input, length);
        std::memset(expected.data() + outputOffset + length, 0, limit - length);
        Require(stpncpy_nid_postfix(output, input, limit) == output + length);
        Require(std::memcmp(destination.data, expected.data(), destination.size) == 0);
    }
    Require(strnlen_nid_postfix(nullptr, 0) == 0);
    Require(strnlen_s_nid_postfix(nullptr, 5) == 0);
    Require(stpncpy_nid_postfix(nullptr, nullptr, 0) == nullptr);
    const char text[] = "abc";
    Require(strnlen_nid_postfix(text, SIZE_MAX) == 3);
}
