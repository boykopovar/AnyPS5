#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

extern "C" {
int APS5_VABI sceGnmSetEmbeddedVsShader(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t);
int APS5_VABI sceGnmSetEmbeddedPsShader(std::uint32_t*, std::uint32_t, std::uint32_t, std::uint32_t);
}

namespace {

constexpr int Failure = static_cast<int>(0x8EEE00FFu);
constexpr std::uint32_t Untouched = 0xa5a5a5a5u;
constexpr std::uint32_t BufferWords = 48;

std::uint32_t g_words[BufferWords];

void Require(bool value) { if (!value) std::abort(); }

std::uint32_t* Fresh() {
    for (auto& word : g_words) word = Untouched;
    return g_words;
}

void RequireAt(std::uint32_t at, std::initializer_list<std::uint32_t> expected) {
    for (const auto word : expected) Require(g_words[at++] == word);
}

void RequireUntouchedFrom(std::uint32_t at) {
    for (; at < BufferWords; ++at) Require(g_words[at] == Untouched);
}

const std::uint32_t* ShaderCode() {
    const auto address = (static_cast<std::uintptr_t>(g_words[3]) << 40) | (static_cast<std::uintptr_t>(g_words[2]) << 8);
    return reinterpret_cast<const std::uint32_t*>(address);
}

void CheckVs() {
    Require(sceGnmSetEmbeddedVsShader(Fresh(), 0x1d, 0, 0) == 0);
    RequireAt(0, {0xc0027600u, 0x48});
    RequireAt(4, {0xc0027600u, 0x4a, 0xc0000, 4, 0xc0016900u, 0x207, 0, 0xc0016900u, 0x1b1, 0, 0xc0016900u, 0x1c3, 4,
        0xc00a1000u, 0});
    RequireUntouchedFrom(19);
    const auto* code = ShaderCode();
    Require(code[0] == 0xbeeb0387u && code[10] == 0xf80008cfu && code[11] == 0x03020001u && code[14] == 0xbf810000u);

    Require(sceGnmSetEmbeddedVsShader(Fresh(), 0x1d, 1, 0) == Failure);
    RequireUntouchedFrom(0);
}

void CheckPs() {
    Require(sceGnmSetEmbeddedPsShader(Fresh(), 0x28, 0, 0) == 0);
    RequireAt(0, {0xc0027600u, 8});
    RequireAt(4, {0xc0027600u, 10, 0xc0000, 4, 0xc0026900u, 0x1c4, 0, 4, 0xc0026900u, 0x1b3, 2, 2, 0xc0016900u, 0x1b6, 0,
        0xc0016900u, 0x1b8, 0, 0xc0016900u, 0x203, 0x10, 0xc0016900u, 0x8f, 0xf, 0xc00a1000u, 0});
    RequireUntouchedFrom(30);
    const auto* black = ShaderCode();
    Require(black[0] == 0xbeeb0383u && black[2] == 0x5e000100u && black[4] == 0xf8001c0fu && black[6] == 0xbf810000u);

    Require(sceGnmSetEmbeddedPsShader(Fresh(), 0x28, 1, 0) == 0);
    RequireAt(4, {0xc0027600u, 10, 0x2000, 0, 0xc0026900u, 0x1c4, 0, 2, 0xc0026900u, 0x1b3, 2, 2, 0xc0016900u, 0x1b6, 0,
        0xc0016900u, 0x1b8, 0, 0xc0016900u, 0x203, 0x10, 0xc0016900u, 0x8f, 3, 0xc00a1000u, 0});
    RequireUntouchedFrom(30);
    const auto* depthOnly = ShaderCode();
    Require(depthOnly != black && depthOnly[1] == 0x7e040280u && depthOnly[2] == 0xf8001803u && depthOnly[4] == 0xbf810000u);

    Require(sceGnmSetEmbeddedPsShader(Fresh(), 0x28, 2, 0) == Failure);
    RequireUntouchedFrom(0);
}

} // namespace

int main() {
    CheckVs();
    CheckPs();
}
