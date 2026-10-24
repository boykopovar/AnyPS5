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

int main() {
    TestGetModule();
}
