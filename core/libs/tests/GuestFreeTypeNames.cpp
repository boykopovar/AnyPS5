#include "FontFixture.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

using namespace FreeType;

extern "C" {
int APS5_VABI FT_Get_Glyph_Name_nid_postfix(GuestFaceRec*, std::uint32_t, void*, std::uint32_t);
std::uint32_t APS5_VABI FT_Get_Name_Index_nid_postfix(GuestFaceRec*, const char*);
const char* APS5_VABI FT_Get_Postscript_Name_nid_postfix(GuestFaceRec*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType names check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static std::vector<unsigned char> NamedFont(const std::string& postscript, const std::string& first, const std::string& second) {
    std::vector<unsigned char> post(32, 0);
    post[1] = 2;
    for (const int value : {3, 0, 258, 259}) FontFixture::Put16(post, value);
    for (const auto* name : {&first, &second}) {
        post.push_back(static_cast<unsigned char>(name->size()));
        post.insert(post.end(), name->begin(), name->end());
    }
    auto names = FontFixture::Words({0, 1, 18, 3, 1, 0x0409, 6, static_cast<int>(postscript.size() * 2), 0});
    for (const unsigned char ch : postscript) FontFixture::Put16(names, ch);
    return FontFixture::BuildFont(3, {{"post", post}, {"name", names}});
}

struct FacePair {
    std::vector<unsigned char> bytes;
    GuestFace guest;
    FT_Face oracle = nullptr;

    FacePair(GuestLibrary& library, FT_Library nativeLibrary, std::vector<unsigned char> font) : bytes(std::move(font)) {
        guest.library = &library;
        Require(FT_New_Memory_Face(library.host, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &guest.host) == 0);
        Require(FT_New_Memory_Face(nativeLibrary, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &oracle) == 0);
        SyncFace(guest);
    }

    ~FacePair() {
        Require(FT_Done_Face(guest.host) == 0);
        Require(FT_Done_Face(oracle) == 0);
    }
};

static void CompareGlyph(GuestFaceRec* guest, FT_Face native, std::uint32_t index, std::uint32_t size) {
    std::array<char, 32> expected;
    expected.fill('?');
    auto actual = expected;
    const FT_Error error = FT_Get_Glyph_Name(native, index, expected.data() + 3, size);
    Require(FT_Get_Glyph_Name_nid_postfix(guest, index, actual.data() + 3, size) == error);
    Require(actual == expected);
}

static void CheckFace(FacePair& face, bool names) {
    Require((FT_HAS_GLYPH_NAMES(face.oracle) != 0) == names);
    for (const std::uint32_t index : {0u, 1u, 2u, 3u, 0x7FFFFFFFu}) {
        for (const std::uint32_t size : {0u, 1u, 2u, 5u, 24u}) CompareGlyph(&face.guest.rec, face.oracle, index, size);
        Require(FT_Get_Glyph_Name_nid_postfix(&face.guest.rec, index, nullptr, 24) == FT_Get_Glyph_Name(face.oracle, index, nullptr, 24));
    }
    for (const std::uint32_t index : {0x80000000u, 0xFFFFFFFFu}) {
        for (const std::uint32_t size : {0u, 1u, 24u}) {
            std::array<char, 32> expected;
            expected.fill('?');
            auto actual = expected;
            if (size) expected[3] = '\0';
            Require(FT_Get_Glyph_Name_nid_postfix(&face.guest.rec, index, actual.data() + 3, size) ==
                    (size ? FT_Err_Invalid_Glyph_Index : FT_Err_Invalid_Argument));
            Require(actual == expected);
        }
    }
    for (const char* name : {".notdef", "alpha.custom", "beta", "gamma.custom", "delta", "ALPHA.CUSTOM", "missing", ""}) {
        Require(FT_Get_Name_Index_nid_postfix(&face.guest.rec, name) == FT_Get_Name_Index(face.oracle, name));
    }
    Require(FT_Get_Name_Index_nid_postfix(&face.guest.rec, nullptr) == 0);
    const char* actual = FT_Get_Postscript_Name_nid_postfix(&face.guest.rec);
    const char* expected = FT_Get_Postscript_Name(face.oracle);
    Require((actual == nullptr) == (expected == nullptr));
    Require(!actual || std::strcmp(actual, expected) == 0);
    Require(actual == FT_Get_Postscript_Name(face.guest.host));
}

int main() {
    GuestLibrary library{};
    FT_Library nativeLibrary = nullptr;
    Require(FT_Init_FreeType(&library.host) == 0);
    Require(FT_Init_FreeType(&nativeLibrary) == 0);
    {
        FacePair first(library, nativeLibrary, NamedFont("NameFixture-A", "alpha.custom", "beta"));
        FacePair second(library, nativeLibrary, NamedFont("NameFixture-B", "gamma.custom", "delta"));
        std::vector<unsigned char> post(32, 0);
        post[1] = 3;
        FacePair unnamed(library, nativeLibrary, FontFixture::BuildFont(3, {{"post", post}}));
        CheckFace(first, true);
        Require(FT_Get_Name_Index_nid_postfix(&first.guest.rec, "alpha.custom") == 1);
        Require(FT_Get_Name_Index_nid_postfix(&first.guest.rec, "beta") == 2);
        const char* retained = FT_Get_Postscript_Name_nid_postfix(&first.guest.rec);
        Require(retained && std::strcmp(retained, "NameFixture-A") == 0);
        CheckFace(second, true);
        Require(FT_Get_Name_Index_nid_postfix(&second.guest.rec, "gamma.custom") == 1);
        Require(FT_Get_Name_Index_nid_postfix(&second.guest.rec, "delta") == 2);
        CheckFace(unnamed, false);
        Require(FT_Get_Postscript_Name_nid_postfix(&unnamed.guest.rec) == nullptr);
        Require(FT_Get_Postscript_Name_nid_postfix(&first.guest.rec) == retained && std::strcmp(retained, "NameFixture-A") == 0);
        for (const std::uint32_t size : {0u, 1u, 24u}) CompareGlyph(nullptr, nullptr, 0, size);
        Require(FT_Get_Glyph_Name_nid_postfix(nullptr, 0, nullptr, 0) == FT_Err_Invalid_Face_Handle);
        Require(FT_Get_Name_Index_nid_postfix(nullptr, "alpha.custom") == 0);
        Require(FT_Get_Name_Index_nid_postfix(nullptr, nullptr) == 0);
        Require(FT_Get_Postscript_Name_nid_postfix(nullptr) == nullptr);
    }
    Require(FT_Done_FreeType(library.host) == 0);
    Require(FT_Done_FreeType(nativeLibrary) == 0);
}
