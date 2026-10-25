#include "prx/libSceSulphaDrv/libSceSulphaDrv.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

int main() {
    errno = 7;
    Require(sceSulphaGetDefaultConfig(nullptr) == static_cast<int>(0x804d0003u) && errno == 7,
            "A null configuration must return the native argument error without changing errno");
    struct Config {
        std::uint64_t size;
        std::uint64_t capacity;
        std::uint32_t setting;
        std::uint32_t port;
    };
    static_assert(sizeof(Config) == 24);
    for (const auto shift : {0u, 1u}) for (const auto initial : std::array<std::uint64_t, 4>{0, 24, 560, UINT64_MAX}) {
        for (const unsigned char pattern : {0xa5, 0x5a}) {
            std::array<unsigned char, 48> storage;
            storage.fill(pattern);
            auto* config = storage.data() + 8 + shift;
            std::memcpy(config, &initial, sizeof(initial));
            errno = 7;
            Require(sceSulphaGetDefaultConfig(config) == 0 && errno == 7,
                    "Default configuration must accept non-null storage regardless of its incoming size");
            Config result;
            std::memcpy(&result, config, sizeof(result));
            Require(result.size == 24 && result.capacity == 4194304 && result.setting == 3 && result.port == 4001,
                    "Default configuration must match the native 24-byte record");
            for (std::size_t i = 0; i < storage.size(); ++i) {
                if (i < 8 + shift || i >= 8 + shift + sizeof(Config))
                    Require(storage[i] == pattern, "Default configuration must preserve bytes outside its record");
            }
        }
    }
}
