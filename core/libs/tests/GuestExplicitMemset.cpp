#include "prx/libc/include/Api.hpp"
#include <array>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Check(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "memset_explicit check failed at line %d\n", line);
    std::abort();
}
#define Require(condition) Check((condition), __LINE__)

class GuardedPage {
    unsigned char* allocation;
public:
    std::size_t size;
    unsigned char* data;

    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        allocation = static_cast<unsigned char*>(VirtualAlloc(nullptr, size * 3, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
        Require(allocation != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(allocation + size, size, PAGE_READWRITE, &previous) != 0);
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0);
        size = static_cast<std::size_t>(pageSize);
        allocation = static_cast<unsigned char*>(mmap(nullptr, size * 3, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(allocation != MAP_FAILED);
        Require(mprotect(allocation + size, size, PROT_READ | PROT_WRITE) == 0);
#endif
        data = allocation + size;
    }

    GuardedPage(const GuardedPage&) = delete;
    GuardedPage& operator=(const GuardedPage&) = delete;

    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(allocation, 0, MEM_RELEASE);
#else
        munmap(allocation, size * 3);
#endif
    }
};

static void Invoke(void* destination, int value, std::size_t count) {
    *__error_nid_postfix() = 97;
    Require(memset_explicit_nid_postfix(destination, value, count) == destination);
    Require(*__error_nid_postfix() == 97);
}

static void Verify(unsigned char* bytes, std::size_t size, std::size_t offset, int value, std::size_t count) {
    Require(offset <= size && count <= size - offset);
    for (std::size_t index = 0; index < size; ++index) bytes[index] = 0xA7;
    Invoke(bytes + offset, value, count);
    for (std::size_t index = 0; index < size; ++index) {
        const auto expected = index >= offset && index - offset < count ? static_cast<unsigned char>(value) : 0xA7;
        Require(bytes[index] == expected);
    }
}

static void Empty() {
    GuardedPage page;
    unsigned char byte = 0x35;
    for (const int value : {0, -1, INT_MIN, INT_MAX}) {
        Invoke(nullptr, value, 0);
        Invoke(&byte, value, 0);
        Invoke(page.data - 1, value, 0);
        Invoke(page.data + page.size, value, 0);
        Require(byte == 0x35);
    }
}

static void Values() {
    std::array<unsigned char, 1088> bytes;
    constexpr std::array<std::size_t, 24> lengths{0, 1, 2, 3, 7, 8, 15, 16, 17, 31, 32, 63,
        64, 65, 127, 128, 255, 256, 257, 511, 512, 1023, 1024, 1056};
    const auto checkValue = [&](int value) {
        for (const auto count : lengths) {
            for (std::size_t offset = 0; offset < 32; ++offset) Verify(bytes.data(), bytes.size(), offset, value, count);
        }
    };
    for (int value = 0; value <= 255; ++value) checkValue(value);
    for (const int value : {-1025, -513, -256, -1, 256, 511, INT_MIN, INT_MAX}) checkValue(value);
}

static void Ranges() {
    std::array<unsigned char, 384> bytes;
    for (std::size_t offset = 0; offset < 64; ++offset) {
        for (std::size_t count = 0; count <= 256; ++count) {
            for (const int value : {0, 0xA7, -1}) Verify(bytes.data(), bytes.size(), offset, value, count);
        }
    }
}

static void GuardPages() {
    GuardedPage page;
    for (std::size_t count = 0; count <= page.size; ++count) {
        for (const int value : {0, 0x35, -1}) {
            Verify(page.data, page.size, 0, value, count);
            Verify(page.data, page.size, page.size - count, value, count);
        }
    }
}

int main(int argc, char** argv) {
    Require(argc == 2);
    if (std::strcmp(argv[1], "empty") == 0) Empty();
    else if (std::strcmp(argv[1], "values") == 0) Values();
    else if (std::strcmp(argv[1], "ranges") == 0) Ranges();
    else if (std::strcmp(argv[1], "guard_pages") == 0) GuardPages();
    else Require(false);
    std::puts("memset_explicit return values, bytes, boundaries and errno passed");
}
