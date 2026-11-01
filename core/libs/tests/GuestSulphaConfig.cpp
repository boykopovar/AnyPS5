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
    for (const int expectedErrno : {0, 7, 22, 42}) {
        errno = expectedErrno;
        Require(sceSulphaGetDefaultConfig(nullptr) == static_cast<int>(0x804d0003u) && errno == expectedErrno,
                "A null configuration must return the native argument error without changing errno");
    }

    struct Config {
        std::uint64_t size;
        std::uint64_t capacity;
        std::uint32_t setting;
        std::uint32_t port;
    };
    static_assert(sizeof(Config) == 24);

    std::array<std::uint8_t, 24> exactStorage;
    exactStorage.fill(0xee);
    errno = 7;
    Require(sceSulphaGetDefaultConfig(exactStorage.data()) == 0 && errno == 7,
            "Default configuration must accept exact 24-byte buffer without changing errno");
    Config exactResult;
    std::memcpy(&exactResult, exactStorage.data(), sizeof(exactResult));
    Require(exactResult.size == 24 && exactResult.capacity == 4194304 && exactResult.setting == 3 && exactResult.port == 4001,
            "Default configuration in exact 24-byte buffer must match expected fields");

    std::uint64_t rawWords[3]{0, 0, 0};
    Require(sceSulphaGetDefaultConfig(rawWords) == 0,
            "Default configuration must accept 64-bit array destination");
    Require(rawWords[0] == 24ULL && rawWords[1] == 0x400000ULL && rawWords[2] == 0xFA100000003ULL,
            "Default configuration raw 64-bit words must match native defaults");

    Config idempotentConfig{};
    Require(sceSulphaGetDefaultConfig(&idempotentConfig) == 0,
            "First call to default configuration must succeed");
    Require(sceSulphaGetDefaultConfig(&idempotentConfig) == 0,
            "Second call to default configuration must succeed idempotently");
    Require(idempotentConfig.size == 24 && idempotentConfig.capacity == 4194304 &&
            idempotentConfig.setting == 3 && idempotentConfig.port == 4001,
            "Default configuration must remain consistent across repeated calls");

    for (const auto shift : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u}) {
        for (const auto initial : std::array<std::uint64_t, 5>{0, 1, 24, 560, UINT64_MAX}) {
            for (const unsigned char pattern : {0x00, 0x5a, 0xa5, 0xff}) {
                std::array<unsigned char, 64> storage;
                storage.fill(pattern);
                auto* config = storage.data() + 16 + shift;
                std::memcpy(config, &initial, sizeof(initial));
                errno = 7;
                Require(sceSulphaGetDefaultConfig(config) == 0 && errno == 7,
                        "Default configuration must accept non-null storage regardless of its incoming size");
                Config result;
                std::memcpy(&result, config, sizeof(result));
                Require(result.size == 24 && result.capacity == 4194304 && result.setting == 3 && result.port == 4001,
                        "Default configuration must match the native 24-byte record");
                for (std::size_t i = 0; i < storage.size(); ++i) {
                    if (i < 16 + shift || i >= 16 + shift + sizeof(Config))
                        Require(storage[i] == pattern, "Default configuration must preserve bytes outside its record");
                }
            }
        }
    }

}
