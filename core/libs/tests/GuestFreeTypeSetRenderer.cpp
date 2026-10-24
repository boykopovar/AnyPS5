#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_MODULE_H
#include FT_RENDER_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>

using namespace FreeType;

extern "C" {
int APS5_VABI FT_Set_Renderer_nid_postfix(GuestLibrary*, void*, std::uint32_t, const GuestParameter*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType set renderer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct Libraries {
    GuestLibrary guest{};
    FT_Library native = nullptr;

    Libraries() { Require(FT_Init_FreeType(&guest.host) == 0 && FT_Init_FreeType(&native) == 0); }
    ~Libraries() { Require(FT_Done_FreeType(guest.host) == 0 && FT_Done_FreeType(native) == 0); }
};

int main() {
    for (const std::uint32_t format : {static_cast<std::uint32_t>(FT_GLYPH_FORMAT_OUTLINE), static_cast<std::uint32_t>(FT_GLYPH_FORMAT_BITMAP)}) {
        Libraries libraries;
        void* guestRenderer = FT_Get_Renderer(libraries.guest.host, static_cast<FT_Glyph_Format>(format));
        FT_Renderer nativeRenderer = FT_Get_Renderer(libraries.native, static_cast<FT_Glyph_Format>(format));
        Require((guestRenderer != nullptr) == (nativeRenderer != nullptr));
        Require(FT_Set_Renderer_nid_postfix(&libraries.guest, guestRenderer, 0, nullptr) == FT_Set_Renderer(libraries.native, nativeRenderer, 0, nullptr));
        Require(FT_Get_Renderer(libraries.guest.host, static_cast<FT_Glyph_Format>(format)) == guestRenderer);
        GuestParameter guestParameters[2] = {{0x12345678, nullptr}, {0x7fffffff, nullptr}};
        FT_Parameter nativeParameters[2] = {{0x12345678, nullptr}, {0x7fffffff, nullptr}};
        for (const std::uint32_t count : {0u, 1u, 2u}) {
            Require(FT_Set_Renderer_nid_postfix(&libraries.guest, guestRenderer, count, guestParameters) ==
                    FT_Set_Renderer(libraries.native, nativeRenderer, count, nativeParameters));
        }
        Require(FT_Set_Renderer_nid_postfix(&libraries.guest, guestRenderer, 1, nullptr) == FT_Set_Renderer(libraries.native, nativeRenderer, 1, nullptr));
        Require(FT_Set_Renderer_nid_postfix(&libraries.guest, nullptr, 0, nullptr) == FT_Set_Renderer(libraries.native, nullptr, 0, nullptr));
        Require(FT_Set_Renderer_nid_postfix(nullptr, guestRenderer, 0, nullptr) == FT_Set_Renderer(nullptr, nativeRenderer, 0, nullptr));
    }
    Libraries first;
    Libraries second;
    void* foreign = FT_Get_Renderer(second.guest.host, FT_GLYPH_FORMAT_OUTLINE);
    FT_Renderer nativeForeign = FT_Get_Renderer(second.native, FT_GLYPH_FORMAT_OUTLINE);
    Require(FT_Set_Renderer_nid_postfix(&first.guest, foreign, 0, nullptr) == FT_Set_Renderer(first.native, nativeForeign, 0, nullptr));
    GuestParameter wide[1] = {{0xFFFFFFFFFFFFFFFFull, nullptr}};
    FT_Parameter nativeWide[1] = {{static_cast<FT_ULong>(0xFFFFFFFFFFFFFFFFull), nullptr}};
    if (sizeof(FT_ULong) == 8) {
        Require(FT_Set_Renderer_nid_postfix(&first.guest, FT_Get_Renderer(first.guest.host, FT_GLYPH_FORMAT_OUTLINE), 1, wide) ==
                FT_Set_Renderer(first.native, FT_Get_Renderer(first.native, FT_GLYPH_FORMAT_OUTLINE), 1, nativeWide));
    }
}
