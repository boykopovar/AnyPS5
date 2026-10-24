#include "FontFixture.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_MULTIPLE_MASTERS_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <utility>
#include <vector>

using namespace FreeType;

extern "C" {
int APS5_VABI FT_Get_MM_Var_nid_postfix(GuestFaceRec*, void**);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType variation check failed at line %d\n", line);
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

static Libraries& Shared() {
    static Libraries libraries;
    return libraries;
}

static std::vector<unsigned char> VariableFont() {
    const std::vector<unsigned char> fvar = FontFixture::Words({1, 0, 16, 2, 1, 20, 0, 8, 0x7767, 0x6874, 0x0064, 0, 0x0190, 0, 0x0384, 0, 0, 256});
    const std::vector<unsigned char> gvar = FontFixture::Words({1, 0, 1, 0, 0, 0, 3, 0, 0, 28, 0, 0, 0, 0});
    return FontFixture::BuildFont(3, {{"fvar", fvar}, {"gvar", gvar}});
}

struct FacePair {
    std::vector<unsigned char> bytes;
    GuestFace guest;
    FT_Face oracle = nullptr;

    explicit FacePair(std::vector<unsigned char> font) : bytes(std::move(font)) {
        Libraries& libraries = Shared();
        guest.library = &libraries.guest;
        Require(FT_New_Memory_Face(libraries.guest.host, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &guest.host) == 0);
        Require(FT_New_Memory_Face(libraries.native, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &oracle) == 0);
        SyncFace(guest);
    }

    ~FacePair() {
        Require(FT_Done_Face(guest.host) == 0);
        Require(FT_Done_Face(oracle) == 0);
    }
};

static void SameVariations(const FT_MM_Var* actual, const FT_MM_Var* expected) {
    Require(actual->num_axis == expected->num_axis && actual->num_designs == expected->num_designs && actual->num_namedstyles == expected->num_namedstyles);
    for (FT_UInt index = 0; index < expected->num_axis; ++index) {
        const FT_Var_Axis& left = actual->axis[index];
        const FT_Var_Axis& right = expected->axis[index];
        Require(left.minimum == right.minimum && left.def == right.def && left.maximum == right.maximum && left.tag == right.tag && left.strid == right.strid);
        Require((left.name == nullptr) == (right.name == nullptr) && (!left.name || std::strcmp(left.name, right.name) == 0));
    }
}

static void TestGetMMVar() {
    FacePair pair(VariableFont());
    void* actual = nullptr;
    FT_MM_Var* expected = nullptr;
    Require(FT_Get_MM_Var_nid_postfix(&pair.guest.rec, &actual) == FT_Get_MM_Var(pair.oracle, &expected));
    Require(actual != nullptr && expected != nullptr);
    SameVariations(static_cast<FT_MM_Var*>(actual), expected);
    FT_Done_MM_Var(Shared().guest.host, static_cast<FT_MM_Var*>(actual));
    FT_Done_MM_Var(Shared().native, expected);
    FacePair plain(FontFixture::BuildFont(3, {}));
    actual = reinterpret_cast<void*>(1);
    expected = nullptr;
    Require(FT_Get_MM_Var_nid_postfix(&plain.guest.rec, &actual) == FT_Get_MM_Var(plain.oracle, &expected));
    Require(FT_Get_MM_Var_nid_postfix(&pair.guest.rec, nullptr) == FT_Get_MM_Var(pair.oracle, nullptr));
    Require(FT_Get_MM_Var_nid_postfix(nullptr, &actual) == FT_Get_MM_Var(nullptr, &expected));
}

int main() {
    TestGetMMVar();
}
