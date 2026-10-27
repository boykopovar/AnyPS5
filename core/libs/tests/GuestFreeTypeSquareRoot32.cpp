#include "prx/libSceFreeType/include/SquareRoot32.hpp"
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "FreeType square root check failed at line %d\n", line);
        std::exit(EXIT_FAILURE);
    }
}
#define Check(value) Require((value), __LINE__)

int main(int count, char** arguments) {
    Check(count == 3);
    const bool patched = std::strcmp(arguments[2], "patched") == 0;
    Check(patched || std::strcmp(arguments[2], "unpatched") == 0);
    const char* symbol = patched ? "C7N2GuwqTA0" : "FT_Sqrt32_nid_postfix";
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
    const auto squareRoot = std::bit_cast<decltype(&FT_Sqrt32_nid_postfix)>(address);
    for (std::uint32_t root = 0; root < 65536; ++root) {
        const std::uint64_t square = std::uint64_t{root} * root;
        Check(squareRoot(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(square))) == static_cast<std::int32_t>(root));
        if (root != 0)
            Check(squareRoot(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(square - 1))) == static_cast<std::int32_t>(root - 1));
        const std::uint64_t upper = (std::uint64_t{root} + 1) * (root + 1) - 1;
        Check(squareRoot(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(upper))) == static_cast<std::int32_t>(root));
    }
    std::uint32_t value = 0x9e3779b9u;
    for (int index = 0; index < 65536; ++index) {
        value = value * 1664525u + 1013904223u;
        const auto result = squareRoot(std::bit_cast<std::int32_t>(value));
        Check(result >= 0 && result <= 65535);
        const auto root = static_cast<std::uint64_t>(result);
        Check(root * root <= value && (root + 1) * (root + 1) > value);
    }
#ifdef _WIN32
    Check(FreeLibrary(module) != 0);
#else
    Check(dlclose(module) == 0);
#endif
}
