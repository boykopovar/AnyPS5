#include "prx/libSceFreeType/include/Api.hpp"
#include "FontFixture.hpp"
#include "Type1Fixture.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
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

static void TestType1Tracks(GuestLibrary& library, FT_Library nativeLibrary) {
    FacePair pair(library, nativeLibrary, Type1Fixture::Font());
    std::string metrics = Type1Fixture::Afm();
    metrics.insert(metrics.find("StartKernPairs"), "StartTrackKern 1\nTrackKern -1 8 -2 16 2\nEndTrackKern\n");
    FT_Open_Args args{};
    args.flags = FT_OPEN_MEMORY;
    args.memory_base = reinterpret_cast<const FT_Byte*>(metrics.data());
    args.memory_size = static_cast<FT_Long>(metrics.size());
    Require(FT_Attach_Stream(pair.guest.host, &args) == 0);
    Require(FT_Attach_Stream(pair.oracle, &args) == 0);

    constexpr std::int64_t sentinel = 0x123456789ALL;
    for (const int points : {6, 10, 12, 18}) {
        const FT_Fixed size = points << 16;
        for (const int degree : {-1, 1}) {
            FT_Fixed native = 37;
            std::int64_t guest = sentinel;
            Require(FT_Get_Track_Kerning(pair.oracle, size, degree, &native) == 0);
            Require(FT_Get_Track_Kerning_nid_postfix(&pair.guest.rec, size, degree, &guest) == 0);
            if (degree == -1) {
                Require(native != 37 && guest == native);
                if (points == 12) Require(guest == 0);
            } else {
                Require(native == 37 && guest == sentinel);
            }
        }
    }
}

int main() {
    GuestLibrary library{};
    FT_Library nativeLibrary = nullptr;
    Require(FT_Init_FreeType(&library.host) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    TestType1Tracks(library, nativeLibrary);
    {
        FacePair truetype(library, nativeLibrary, FontFixture::BuildFont(3, {}));
        FacePair bitmap(library, nativeLibrary, FontFixture::BitmapFont());
        for (FacePair* face : {&truetype, &bitmap}) {
            for (const std::int64_t pointSize : {0LL, 1LL << 16, 12LL << 16, -(8LL << 16)}) {
                for (const std::int32_t degree : {-1, 0, 1, 3}) Compare(*face, pointSize, degree);
            }
            std::int64_t value = 0;
            Require(FT_Get_Track_Kerning_nid_postfix(&face->guest.rec, 12 << 16, 0, nullptr) == FT_Get_Track_Kerning(face->oracle, 12 << 16, 0, nullptr));
            Require(FT_Get_Track_Kerning_nid_postfix(&face->guest.rec, 12 << 16, 0, reinterpret_cast<std::int64_t*>(std::uintptr_t{1})) == FT_Err_Invalid_Argument);
            Require(value == 0);
        }
        std::int64_t value = 0;
        Require(FT_Get_Track_Kerning_nid_postfix(nullptr, 12 << 16, 0, &value) == FT_Err_Invalid_Face_Handle);
        GuestFaceRec empty{};
        Require(FT_Get_Track_Kerning_nid_postfix(&empty, 12 << 16, 0, &value) == FT_Get_Track_Kerning(nullptr, 12 << 16, 0, nullptr));
    }
    Require(FT_Done_FreeType(library.host) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
}
