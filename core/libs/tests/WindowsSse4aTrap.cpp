#include <windows.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr std::size_t PageBytes = 0x1000;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint64_t extract(std::uint64_t value, unsigned length, unsigned index) {
    const std::uint64_t mask = length == 0 ? ~std::uint64_t{0} : (std::uint64_t{1} << length) - 1u;
    return (value >> index) & mask;
}

void runAcrossPageEnd(std::uint8_t* pages, std::size_t inFirstPage, unsigned length, unsigned index) {
    const std::array<std::uint8_t, 4> load{0xf3, 0x0f, 0x7e, 0x01};
    const std::array<std::uint8_t, 6> extrq{0x66, 0x0f, 0x78, 0xc0, static_cast<std::uint8_t>(length), static_cast<std::uint8_t>(index)};
    const std::array<std::uint8_t, 5> storeAndReturn{0x66, 0x0f, 0xd6, 0x01, 0xc3};
    std::uint8_t* const start = pages + PageBytes - inFirstPage - load.size();
    std::memset(pages, 0xcc, 2 * PageBytes);
    std::memcpy(start, load.data(), load.size());
    std::memcpy(start + load.size(), extrq.data(), extrq.size());
    std::memcpy(start + load.size() + extrq.size(), storeAndReturn.data(), storeAndReturn.size());
    FlushInstructionCache(GetCurrentProcess(), pages, 2 * PageBytes);
    const std::uint64_t input = 0x1122334455667788ull;
    std::uint64_t value = input;
    reinterpret_cast<void (*)(std::uint64_t*)>(start)(&value);
    char line[160];
    std::snprintf(line, sizeof(line), "extrq with %zu of its 6 bytes before the page end (length %u, index %u) gave 0x%016llx, expected 0x%016llx", inFirstPage, length, index, static_cast<unsigned long long>(value), static_cast<unsigned long long>(extract(input, length, index)));
    require(value == extract(input, length, index), line);
}

}

int main() {
    try {
        auto* const pages = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 2 * PageBytes, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
        require(pages != nullptr, "cannot allocate executable pages");
        for (std::size_t inFirstPage = 0; inFirstPage <= 6; ++inFirstPage) {
            runAcrossPageEnd(pages, inFirstPage, 16, 8);
            runAcrossPageEnd(pages, inFirstPage, 0, 0);
        }
        VirtualFree(pages, 0, MEM_RELEASE);
        std::puts("SSE4a trap tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAILED: %s\n", error.what());
        return 1;
    }
}
