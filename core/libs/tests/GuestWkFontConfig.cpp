#include "prx/libSceWkFontConfig/libSceWkFontConfig.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

struct FontAttr {
    std::uint32_t size;
    std::uint32_t field4;
    std::uint32_t field8;
    std::uint32_t fieldC;
    std::uint32_t field10;
};

} // namespace

int main() {
    constexpr int invalidArgument = static_cast<int>(0x813C0001u);
    const void* fontOut = nullptr;
    char text[64] = {};
    const char name[] = "font";
    int fontTarget = 0;
    for (int call = 0; call < 2; ++call) {
        Require(sceWkFontConfigGetFont(nullptr, 0, nullptr) == invalidArgument);
        Require(sceWkFontConfigGetFont(&fontOut, 0x96, nullptr) == invalidArgument);
        Require(sceWkFontConfigGetFont(&fontOut, 0xFFFFFFFFu, nullptr) == invalidArgument);
        for (const FontAttr& attr : std::array<FontAttr, 6>{FontAttr{0x10, 0, 0, 0, 0}, FontAttr{0x14, 7, 0, 0, 0},
                                                             FontAttr{0x14, 0, 10, 0, 0}, FontAttr{0x14, 0, 0, 3, 0},
                                                             FontAttr{0x14, 0, 0, 0, 3},
                                                             FontAttr{0x14, 0xFFFFFFFFu, 0, 0, 0}}) {
            Require(sceWkFontConfigGetFont(&fontOut, 0, &attr) == invalidArgument);
        }
        Require(sceWkFontConfigGetFontFromFontName(nullptr, name) == invalidArgument);
        Require(sceWkFontConfigGetFontFromFontName(&fontOut, nullptr) == invalidArgument);
        Require(sceWkFontConfigGetFontName(nullptr, text, sizeof(text)) == invalidArgument);
        Require(sceWkFontConfigGetFontName(&fontTarget, nullptr, sizeof(text)) == invalidArgument);
        Require(sceWkFontConfigGetFontPath(nullptr, text, sizeof(text)) == invalidArgument);
        Require(sceWkFontConfigGetFontPath(&fontTarget, nullptr, sizeof(text)) == invalidArgument);
        Require(sceWkFontConfigGetAttr(&fontTarget, nullptr) == invalidArgument);
        for (std::uint32_t size : std::array<std::uint32_t, 3>{0u, 0x13u, 0x15u}) {
            FontAttr attr{size, 0, 0, 0, 0};
            Require(sceWkFontConfigGetAttr(&fontTarget, &attr) == invalidArgument);
        }
    }
    return 0;
}
