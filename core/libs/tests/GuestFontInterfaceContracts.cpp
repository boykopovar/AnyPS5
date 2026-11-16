#include "prx/libSceFont/libSceFont.hpp"
#include "prx/libSceFont/include/FontDriver.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

static bool OutlineCases() {
    if (sceFontOutlineVertexesInterfaceInit(nullptr, nullptr, nullptr, 1) != SCE_FONT_ERROR_INVALID_PARAMETER) return false;
    constexpr std::uint64_t counts[] = {0, 1, 2, 3, 4, 0x100000001ull, 0xffffffffffffffffull};
    for (const auto count : counts) {
        for (unsigned variant = 0; variant < 3; ++variant) {
            alignas(16) std::array<std::byte, 0x50> memory;
            memory.fill(std::byte{0xcc});
            auto expected = memory;
            void* first = variant == 0 ? nullptr : memory.data();
            void* second = variant == 2 ? nullptr : memory.data() + 1;
            const bool valid = first && count >= 1 && count <= 3;
            std::memset(expected.data() + 0x10, 0, 0x30);
            if (valid) {
                const std::uint64_t tag = 0xf08;
                std::memcpy(expected.data() + 0x10, &tag, 8);
                std::memcpy(expected.data() + 0x18, &first, 8);
                std::memcpy(expected.data() + 0x20, &second, 8);
                std::memcpy(expected.data() + 0x28, &count, 8);
            }
            const int result = sceFontOutlineVertexesInterfaceInit(memory.data() + 0x10, first, second, count);
            if (result != (valid ? 0 : SCE_FONT_ERROR_INVALID_PARAMETER) || memory != expected) return false;
        }
    }
    return true;
}

static bool UpdationCases() {
    if (sceFontSupportSystemUpdation(nullptr, 0x20230801) != SCE_FONT_ERROR_INVALID_LIBRARY) return false;
    for (const std::uint32_t badMagic : {0x0u, 0xf00u, 0xf02u}) {
        Font::FontLibNative library{};
        library.magic = badMagic;
        library.reserved0 = 0x8041;
        library.lock_word = 0x1234;
        const auto invalid = library;
        if (sceFontSupportSystemUpdation(&library, 0x20230801) != SCE_FONT_ERROR_INVALID_LIBRARY ||
            std::memcmp(&library, &invalid, sizeof(library))) return false;
    }
    Font::FontLibNative library{};
    library.magic = 0xf01;
    library.reserved0 = 0x8041;
    library.lock_word = 0x1234;
    for (std::uint32_t version : {0u, 0x20230800u, 0x20230801u, 0x20230802u, 0xffffffffu}) {
        auto expected = library;
        const bool supported = version == 0x20230801;
        if (supported) expected.reserved0 |= 0x0200;
        const int result = sceFontSupportSystemUpdation(&library, version);
        if (result != (supported ? 0 : SCE_FONT_ERROR_INVALID_PARAMETER) ||
            std::memcmp(&library, &expected, sizeof(library))) return false;
    }
    const auto enabled = library;
    if (sceFontSupportSystemUpdation(&library, 0x20230801) != 0 ||
        std::memcmp(&library, &enabled, sizeof(library))) return false;
    return true;
}

int main() {
    if (!OutlineCases()) { std::fputs("outline interface contract failed\n", stderr); return 1; }
    if (!UpdationCases()) { std::fputs("system updation contract failed\n", stderr); return 1; }
}
