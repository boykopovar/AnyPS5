#include "prx/libc/include/General.hpp"
#include "Testing.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static std::uint8_t* MapPages(std::size_t count) {
#ifdef _WIN32
    auto* pages = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, count * 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(pages != nullptr);
#else
    void* mapped = mmap(nullptr, count * 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapped != MAP_FAILED);
    auto* pages = static_cast<std::uint8_t*>(mapped);
#endif
    return pages;
}

static void Protect(std::uint8_t* page) {
#ifdef _WIN32
    DWORD old = 0;
    Require(VirtualProtect(page, 4096, PAGE_NOACCESS, &old));
#else
    Require(mprotect(page, 4096, PROT_NONE) == 0);
#endif
}

static void Unmap(std::uint8_t* pages, std::size_t count) {
#ifdef _WIN32
    (void)count;
    VirtualFree(pages, 0, MEM_RELEASE);
#else
    munmap(pages, count * 4096);
#endif
}

static void StridedRows() {
    const std::array<std::uint8_t, 20> source{1, 2, 3, 0, 0, 4, 5, 6, 0, 0, 7, 8, 9, 0, 0, 10, 11, 12, 0, 0};
    std::array<std::uint8_t, 32> destination{};
    destination.fill(0xEE);
    Require(GuestCopyOutRows_nid_no_patch(destination.data(), 8, source.data(), 5, 3, 4));
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t column = 0; column < 8; ++column) {
            const std::uint8_t expected = column < 3 ? static_cast<std::uint8_t>(row * 3 + column + 1) : 0xEE;
            Require(destination[row * 8 + column] == expected);
        }
    }
}

static void ManyRows() {
    constexpr std::size_t Rows = 1500;
    std::vector<std::uint8_t> source(Rows * 16);
    for (std::size_t index = 0; index < source.size(); ++index) source[index] = static_cast<std::uint8_t>(index * 7 + 1);
    std::vector<std::uint8_t> destination(Rows * 32, 0xEE);
    Require(GuestCopyOutRows_nid_no_patch(destination.data(), 32, source.data(), 16, 16, Rows));
    for (std::size_t row = 0; row < Rows; ++row) {
        Require(std::memcmp(destination.data() + row * 32, source.data() + row * 16, 16) == 0);
        for (std::size_t column = 16; column < 32; ++column) Require(destination[row * 32 + column] == 0xEE);
    }
}

static void EmptyCopies() {
    std::array<std::uint8_t, 4> destination{9, 9, 9, 9};
    const std::array<std::uint8_t, 4> source{1, 2, 3, 4};
    Require(GuestCopyOutRows_nid_no_patch(destination.data(), 4, source.data(), 4, 4, 0));
    Require(GuestCopyOutRows_nid_no_patch(destination.data(), 4, source.data(), 4, 0, 1));
    Require(GuestCopyOutRows_nid_no_patch(nullptr, 4, source.data(), 4, 0, 1));
    Require((destination == std::array<std::uint8_t, 4>{9, 9, 9, 9}));
}

static void RowsAroundAHole() {
    std::uint8_t* pages = MapPages(3);
    Protect(pages + 4096);
    std::memset(pages, 0xEE, 4096);
    std::memset(pages + 8192, 0xEE, 4096);
    const std::array<std::uint8_t, 32> source{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
    Require(GuestCopyOutRows_nid_no_patch(pages, 8192, source.data(), 16, 16, 2));
    Require(std::memcmp(pages, source.data(), 16) == 0 && std::memcmp(pages + 8192, source.data() + 16, 16) == 0);
    Require(pages[16] == 0xEE && pages[8192 + 16] == 0xEE);
    errno = 71;
    Require(!GuestCopyOutRows_nid_no_patch(pages + 4096 - 16, 16, source.data(), 16, 16, 2));
    Require(errno == 71);
    Unmap(pages, 3);
}

static void UnwritableDestinations() {
    const std::array<std::uint8_t, 16> source{};
    errno = 71;
    Require(!GuestCopyOutRows_nid_no_patch(reinterpret_cast<void*>(0x10), 16, source.data(), 16, 16, 1));
    Require(!GuestCopyOutRows_nid_no_patch(nullptr, 16, source.data(), 16, 16, 1));
    Require(!GuestCopyOutRows_nid_no_patch(reinterpret_cast<void*>(UINTPTR_MAX - 8), 16, source.data(), 16, 16, 1));
    Require(!GuestCopyOutRows_nid_no_patch(reinterpret_cast<void*>(0x1000), SIZE_MAX / 2, source.data(), 16, 16, 3));
    Require(errno == 71);
}

int main() {
    StridedRows();
    ManyRows();
    EmptyCopies();
    RowsAroundAHole();
    UnwritableDestinations();
    return 0;
}
