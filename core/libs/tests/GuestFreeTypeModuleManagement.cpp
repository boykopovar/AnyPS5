#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_MODULE_H
#include FT_RENDER_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

using namespace FreeType;

extern "C" {
void* APS5_VABI FT_Get_Module_nid_postfix(GuestLibrary*, const char*);
void* APS5_VABI FT_Get_Renderer_nid_postfix(GuestLibrary*, std::uint32_t);
int APS5_VABI FT_Remove_Module_nid_postfix(GuestLibrary*, void*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType module check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct Libraries {
    GuestLibrary guest{};
    FT_Library native = nullptr;

    Libraries() { Require(FT_Init_FreeType(&guest.host) == 0 && FT_Init_FreeType(&native) == 0); }
    ~Libraries() { Require(FT_Done_FreeType(guest.host) == 0 && FT_Done_FreeType(native) == 0); }
    Libraries(const Libraries&) = delete;
    Libraries& operator=(const Libraries&) = delete;
};

static void TestGetModule() {
    Libraries libraries;
    for (const char* name : {"truetype", "cff", "type1", "bdf", "sfnt", "smooth", "raster1", "psnames", "missing", ""}) {
        void* actual = FT_Get_Module_nid_postfix(&libraries.guest, name);
        FT_Module expected = FT_Get_Module(libraries.native, name);
        Require((actual != nullptr) == (expected != nullptr));
        Require(actual == FT_Get_Module(libraries.guest.host, name));
    }
    Require(FT_Get_Module_nid_postfix(nullptr, "truetype") == FT_Get_Module(nullptr, "truetype"));
}

static void TestGetRenderer() {
    Libraries libraries;
    for (const std::uint32_t format : {static_cast<std::uint32_t>(FT_GLYPH_FORMAT_NONE), static_cast<std::uint32_t>(FT_GLYPH_FORMAT_COMPOSITE),
                                       static_cast<std::uint32_t>(FT_GLYPH_FORMAT_BITMAP), static_cast<std::uint32_t>(FT_GLYPH_FORMAT_OUTLINE),
                                       static_cast<std::uint32_t>(FT_GLYPH_FORMAT_PLOTTER), 0x12345678u}) {
        void* actual = FT_Get_Renderer_nid_postfix(&libraries.guest, format);
        FT_Renderer expected = FT_Get_Renderer(libraries.native, static_cast<FT_Glyph_Format>(format));
        Require((actual != nullptr) == (expected != nullptr));
        Require(actual == FT_Get_Renderer(libraries.guest.host, static_cast<FT_Glyph_Format>(format)));
    }
    Require(FT_Get_Renderer_nid_postfix(nullptr, FT_GLYPH_FORMAT_OUTLINE) == FT_Get_Renderer(nullptr, FT_GLYPH_FORMAT_OUTLINE));
}

static void TestRemoveModule() {
    for (const char* name : {"truetype", "cff", "type1", "smooth", "psnames"}) {
        Libraries libraries;
        void* guestModule = FT_Get_Module(libraries.guest.host, name);
        FT_Module nativeModule = FT_Get_Module(libraries.native, name);
        Require(guestModule != nullptr && nativeModule != nullptr);
        Require(FT_Remove_Module_nid_postfix(&libraries.guest, guestModule) == FT_Remove_Module(libraries.native, nativeModule));
        Require(FT_Get_Module(libraries.guest.host, name) == nullptr && FT_Get_Module(libraries.native, name) == nullptr);
    }
    Libraries libraries;
    Libraries other;
    void* foreign = FT_Get_Module(other.guest.host, "truetype");
    FT_Module nativeForeign = FT_Get_Module(other.native, "truetype");
    Require(FT_Remove_Module_nid_postfix(&libraries.guest, foreign) == FT_Remove_Module(libraries.native, nativeForeign));
    Require(FT_Remove_Module_nid_postfix(&libraries.guest, nullptr) == FT_Remove_Module(libraries.native, nullptr));
    Require(FT_Remove_Module_nid_postfix(nullptr, foreign) == FT_Remove_Module(nullptr, nativeForeign));
}

int main() {
    TestGetModule();
    TestGetRenderer();
    TestRemoveModule();
}
