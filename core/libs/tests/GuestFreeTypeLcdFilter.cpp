#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_LCD_FILTER_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

using namespace FreeType;

extern "C" {
int APS5_VABI FT_Library_SetLcdFilter_nid_postfix(GuestLibrary*, std::int32_t);
int APS5_VABI FT_Library_SetLcdFilterWeights_nid_postfix(GuestLibrary*, unsigned char*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType LCD filter check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

int main() {
    GuestLibrary library{};
    FT_Library native = nullptr;
    Require(FT_Init_FreeType(&library.host) == 0 && FT_Init_FreeType(&native) == 0);
    constexpr std::int32_t filters[] = {FT_LCD_FILTER_NONE, FT_LCD_FILTER_DEFAULT, FT_LCD_FILTER_LIGHT, FT_LCD_FILTER_LEGACY,
                                     FT_LCD_FILTER_LEGACY1, FT_LCD_FILTER_MAX, -1, 99};
    for (const std::int32_t filter : filters) {
        Require(FT_Library_SetLcdFilter_nid_postfix(&library, filter) == FT_Library_SetLcdFilter(native, static_cast<FT_LcdFilter>(filter)));
    }
    std::array<unsigned char, FT_LCD_FILTER_FIVE_TAPS> weights{0x08, 0x4D, 0x56, 0x4D, 0x08};
    Require(FT_Library_SetLcdFilterWeights_nid_postfix(&library, weights.data()) == FT_Library_SetLcdFilterWeights(native, weights.data()));
    Require(FT_Library_SetLcdFilterWeights_nid_postfix(&library, nullptr) == FT_Library_SetLcdFilterWeights(native, nullptr));
    Require(FT_Library_SetLcdFilter_nid_postfix(nullptr, FT_LCD_FILTER_DEFAULT) == FT_Library_SetLcdFilter(nullptr, FT_LCD_FILTER_DEFAULT));
    Require(FT_Library_SetLcdFilterWeights_nid_postfix(nullptr, weights.data()) == FT_Library_SetLcdFilterWeights(nullptr, weights.data()));
    Require(FT_Done_FreeType(library.host) == 0 && FT_Done_FreeType(native) == 0);
}
