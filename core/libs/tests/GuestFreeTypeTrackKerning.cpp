#include "prx/libSceFreeType/include/Api.hpp"
#include "FontFixture.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType track kerning check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

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

static void Compare(FacePair& face, std::int64_t pointSize, std::int32_t degree) {
    constexpr std::int64_t sentinel = 0x123456789ALL;
    std::int64_t actual = sentinel;
    FT_Fixed expected = static_cast<FT_Fixed>(sentinel);
    const int actualError = FT_Get_Track_Kerning_nid_postfix(&face.guest.rec, pointSize, degree, &actual);
    const int expectedError = FT_Get_Track_Kerning(face.oracle, static_cast<FT_Fixed>(pointSize), degree, &expected);
    Require(actualError == expectedError);
    if (expectedError == 0) Require(actual == expected);
    else Require(actual == sentinel);
}

int main() {
    GuestLibrary library{};
    FT_Library nativeLibrary = nullptr;
    Require(FT_Init_FreeType(&library.host) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    {
        FacePair truetype(library, nativeLibrary, FontFixture::BuildFont(3, {}));
        FacePair bitmap(library, nativeLibrary, FontFixture::BitmapFont());
        for (FacePair* face : {&truetype, &bitmap}) {
            for (const std::int64_t pointSize : {0LL, 1LL << 16, 12LL << 16, -(8LL << 16)}) {
                for (const std::int32_t degree : {-1, 0, 1, 3}) Compare(*face, pointSize, degree);
            }
            std::int64_t value = 0;
            Require(FT_Get_Track_Kerning_nid_postfix(&face->guest.rec, 12 << 16, 0, nullptr) == FT_Get_Track_Kerning(face->oracle, 12 << 16, 0, nullptr));
            Require(value == 0);
        }
        std::int64_t value = 0;
        Require(FT_Get_Track_Kerning_nid_postfix(nullptr, 12 << 16, 0, &value) == FT_Err_Invalid_Face_Handle);
        GuestFaceRec empty{};
        Require(FT_Get_Track_Kerning_nid_postfix(&empty, 12 << 16, 0, &value) == FT_Get_Track_Kerning(nullptr, 12 << 16, 0, nullptr));
    }
    Require(FT_Done_FreeType(library.host) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
}
