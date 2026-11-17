#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Testing.hpp"

#include FT_LCD_FILTER_H

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

using namespace FreeType;

template <class F>
static bool Rejects(F call) {
    errno = 71;
    try {
        call();
    } catch (const std::runtime_error&) {
        return errno == 71;
    }
    return false;
}

int main() {
    GuestLibrary library{};
    GuestLibrary uninitialized{};
    FT_Library native = nullptr;
    Require(FT_Init_FreeType(&library.host) == 0 && FT_Init_FreeType(&native) == 0);
    constexpr std::int32_t filters[] = {FT_LCD_FILTER_NONE, FT_LCD_FILTER_DEFAULT, FT_LCD_FILTER_LIGHT, FT_LCD_FILTER_LEGACY,
                                     FT_LCD_FILTER_LEGACY1, FT_LCD_FILTER_MAX, -1, 99, FT_LCD_FILTER_NONE};
    for (const std::int32_t filter : filters) {
        Require(FT_Library_SetLcdFilter_nid_postfix(&library, filter) == FT_Library_SetLcdFilter(native, static_cast<FT_LcdFilter>(filter)));
    }
    std::array<unsigned char, FT_LCD_FILTER_FIVE_TAPS> weights{0x08, 0x4D, 0x56, 0x4D, 0x08};
    Require(FT_Library_SetLcdFilterWeights_nid_postfix(&library, weights.data()) == FT_Library_SetLcdFilterWeights(native, weights.data()));
    Require(FT_Library_SetLcdFilterWeights_nid_postfix(&library, nullptr) == FT_Library_SetLcdFilterWeights(native, nullptr));
    Require(FT_Library_SetLcdFilter_nid_postfix(nullptr, FT_LCD_FILTER_DEFAULT) == FT_Library_SetLcdFilter(nullptr, FT_LCD_FILTER_DEFAULT));
    Require(FT_Library_SetLcdFilter_nid_postfix(&uninitialized, FT_LCD_FILTER_DEFAULT) == FT_Library_SetLcdFilter(nullptr, FT_LCD_FILTER_DEFAULT));
    Require(FT_Library_SetLcdFilterWeights_nid_postfix(nullptr, weights.data()) == FT_Library_SetLcdFilterWeights(nullptr, weights.data()));
    Require(FT_Library_SetLcdFilterWeights_nid_postfix(&uninitialized, weights.data()) == FT_Library_SetLcdFilterWeights(nullptr, weights.data()));
    auto* badLibrary = reinterpret_cast<GuestLibrary*>(std::uintptr_t{0x10});
    auto* badWeights = reinterpret_cast<unsigned char*>(std::uintptr_t{0x10});
    Require(Rejects([&] { FT_Library_SetLcdFilter_nid_postfix(badLibrary, FT_LCD_FILTER_DEFAULT); }));
    Require(Rejects([&] { FT_Library_SetLcdFilterWeights_nid_postfix(badLibrary, weights.data()); }));
    Require(Rejects([&] { FT_Library_SetLcdFilterWeights_nid_postfix(badLibrary, nullptr); }));
    Require(Rejects([&] { FT_Library_SetLcdFilterWeights_nid_postfix(&library, badWeights); }));
    Require(Rejects([&] { FT_Library_SetLcdFilterWeights_nid_postfix(nullptr, badWeights); }));
    Require(FT_Done_FreeType(library.host) == 0 && FT_Done_FreeType(native) == 0);
}
