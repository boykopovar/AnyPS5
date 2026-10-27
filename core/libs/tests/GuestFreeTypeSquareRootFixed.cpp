#include "prx/libSceFreeType/include/SquareRootFixed.hpp"
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "FreeType fixed-point square root check failed at line %d\n", line);
        std::exit(EXIT_FAILURE);
    }
}
#define Check(value) Require((value), __LINE__)

int main(int count, char** arguments) {
    Check(count == 3);
    const bool patched = std::strcmp(arguments[2], "patched") == 0;
    Check(patched || std::strcmp(arguments[2], "unpatched") == 0);
    const char* symbol = patched ? "JgtSc0g3G6E" : "FT_SqrtFixed_nid_postfix";
#ifdef _WIN32
    const auto module = LoadLibraryW(std::filesystem::path(arguments[1]).c_str());
    Check(module != nullptr);
    const auto address = GetProcAddress(module, symbol);
#else
    const auto module = dlopen(arguments[1], RTLD_NOW | RTLD_LOCAL);
    if (module == nullptr) std::fprintf(stderr, "%s\n", dlerror());
    Check(module != nullptr);
    const auto address = dlsym(module, symbol);
#endif
    if (address == nullptr) std::fprintf(stderr, "Missing FreeType entry: %s\n", symbol);
    Check(address != nullptr);
    const auto squareRoot = std::bit_cast<decltype(&FT_SqrtFixed_nid_postfix)>(address);
    Check(squareRoot(0) == 0);
    Check(squareRoot(-1) == 0);
    Check(squareRoot(-65536) == 0);
    Check(squareRoot(std::numeric_limits<std::int32_t>::min()) == 0);
    Check(squareRoot(65536) == 65536);
    Check(squareRoot(262144) == 131072);
    constexpr std::uint32_t largest = 0x7fffffffu;
    std::int32_t previous = 0;
    for (std::uint64_t root = 0;; ++root) {
        const std::uint64_t first = (root * root + 65535u) >> 16;
        if (first > largest) break;
        const std::uint64_t next = ((root + 1) * (root + 1) + 65535u) >> 16;
        if (first == next) continue;
        Check(squareRoot(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(first))) == static_cast<std::int32_t>(root));
        if (first > 0)
            Check(squareRoot(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(first - 1))) == previous);
        if (next - 1 <= largest)
            Check(squareRoot(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(next - 1))) == static_cast<std::int32_t>(root));
        previous = static_cast<std::int32_t>(root);
    }
    std::uint32_t value = 0x9e3779b9u;
    for (int index = 0; index < 65536; ++index) {
        value = value * 1664525u + 1013904223u;
        const auto sample = static_cast<std::int32_t>(value & 0x7fffffffu);
        if (sample <= 0) continue;
        const auto result = squareRoot(sample);
        Check(result >= 0);
        const auto root = static_cast<std::uint64_t>(result);
        const auto scaled = static_cast<std::uint64_t>(sample) << 16;
        Check(root * root <= scaled && (root + 1) * (root + 1) > scaled);
    }
#ifdef _WIN32
    Check(FreeLibrary(module) != 0);
#else
    Check(dlclose(module) == 0);
#endif
}
