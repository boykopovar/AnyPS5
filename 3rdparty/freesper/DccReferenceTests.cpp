#include "dcc_fast_clear.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstdio>

static void Require(bool ok) { if (!ok) std::abort(); }

int main() {
    std::size_t accepted = 0, rejected = 0;
    for (unsigned code = 0; code < 256; ++code) {
        for (unsigned components = 1; components <= 4; ++components) {
            for (bool alphaMsb : {false, true}) {
                std::array<std::uint8_t, 16> metadata;
                metadata.fill(static_cast<std::uint8_t>(code));
                std::array<std::uint8_t, 36> pixels;
                pixels.fill(0xa5);
                auto before = pixels;
                std::uint8_t reported = 0x5a;
                const bool valid = (code == 0 || code == 0xc0 || ((code == 0x40 || code == 0x80) && components >= 3));
                const bool result = prosper::gpu::gfx10_dcc_fast_clear_rgba8(pixels.data() + 4, 7, metadata.data(), metadata.size(), components, alphaMsb, &reported);
                Require(result == valid);
                if (!valid) { Require(pixels == before && reported == 0x5a); ++rejected; continue; }
                ++accepted;
                Require(reported == code);
                for (unsigned i = 0; i < 4; ++i) Require(pixels[i] == 0xa5 && pixels[32 + i] == 0xa5);
                for (unsigned texel = 0; texel < 7; ++texel) {
                    for (unsigned c = 0; c < 4; ++c) {
                        unsigned expected = c < components ? (code >= 0x80 ? 255 : 0) : (c == 3 ? 255 : 0);
                        if (components == 4 && c == (alphaMsb ? 3u : 0u)) expected = (code == 0x40 || code == 0xc0) ? 255 : 0;
                        Require(pixels[4 + texel * 4 + c] == expected);
                    }
                }
                for (unsigned pos = 0; pos < metadata.size(); ++pos) {
                    metadata.fill(static_cast<std::uint8_t>(code));
                    metadata[pos] ^= 1;
                    pixels.fill(0xa5); before = pixels; reported = 0x5a;
                    Require(!prosper::gpu::gfx10_dcc_fast_clear_rgba8(pixels.data() + 4, 7, metadata.data(), metadata.size(), components, alphaMsb, &reported));
                    Require(pixels == before && reported == 0x5a);
                    ++rejected;
                }
            }
        }
    }
    std::array<std::uint8_t, 4> pixels{}, metadata{};
    Require(!prosper::gpu::gfx10_dcc_fast_clear_rgba8(pixels.data(), 1, nullptr, 4, 4, true, nullptr));
    Require(!prosper::gpu::gfx10_dcc_fast_clear_rgba8(pixels.data(), 1, metadata.data(), 0, 4, true, nullptr));
    Require(!prosper::gpu::gfx10_dcc_fast_clear_rgba8(nullptr, 1, metadata.data(), 4, 4, true, nullptr));
    for (unsigned count : {0u, 5u}) Require(!prosper::gpu::gfx10_dcc_fast_clear_rgba8(pixels.data(), 1, metadata.data(), 4, count, true, nullptr));
    std::printf("DCC reference PASS: %zu accepted, %zu rejected, plus invalid pointer/size/component inputs\n", accepted, rejected);
}
