#include "prx/libfreetype/libfreetype.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H
#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

constexpr std::int64_t Untouched = 0x5a5a5a5a;

void Require(bool value) {
    if (!value) std::abort();
}

std::vector<unsigned char> ReadFont(const char* name) {
    std::ifstream file(std::string(FREETYPE_TEST_FONTS) + "/" + name, std::ios::binary);
    std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(file), {}};
    Require(!bytes.empty());
    return bytes;
}

struct Faces {
    FreeType::GuestFaceRec* guest = nullptr;
    FT_Face native = nullptr;
};

Faces Open(FreeType::GuestLibrary* guestLibrary, FT_Library nativeLibrary, const std::vector<unsigned char>& bytes) {
    Faces faces;
    Require(FT_New_Memory_Face_nid_postfix(guestLibrary, bytes.data(), static_cast<std::int64_t>(bytes.size()), 0, &faces.guest) == 0);
    Require(FT_New_Memory_Face(nativeLibrary, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &faces.native) == 0);
    return faces;
}

void CheckBlendCoordinates(const Faces& faces, std::uint32_t count) {
    std::array<std::int64_t, 4> guest;
    std::array<FT_Fixed, 4> native;
    guest.fill(Untouched);
    native.fill(static_cast<FT_Fixed>(Untouched));
    const auto guestError = FT_Get_MM_Blend_Coordinates_nid_postfix(faces.guest, count, guest.data());
    const auto nativeError = FT_Get_MM_Blend_Coordinates(faces.native, count, native.data());
    Require(guestError == nativeError);
    for (std::size_t index = 0; index < guest.size(); ++index) Require(guest[index] == native[index]);
    Require(FT_Get_MM_Blend_Coordinates_nid_postfix(faces.guest, count, nullptr) == FT_Get_MM_Blend_Coordinates(faces.native, count, nullptr));
}

void CheckWeightVector(const Faces& faces, FT_UInt count) {
    std::array<std::int64_t, 4> guest;
    std::array<FT_Fixed, 4> native;
    guest.fill(Untouched);
    native.fill(static_cast<FT_Fixed>(Untouched));
    std::uint32_t guestLength = count;
    FT_UInt nativeLength = count;
    Require(FT_Get_MM_WeightVector_nid_postfix(faces.guest, &guestLength, guest.data()) == FT_Get_MM_WeightVector(faces.native, &nativeLength, native.data()));
    Require(guestLength == nativeLength);
    for (std::size_t index = 0; index < guest.size(); ++index) Require(guest[index] == native[index]);
    guestLength = nativeLength = count;
    Require(FT_Get_MM_WeightVector_nid_postfix(faces.guest, &guestLength, nullptr) == FT_Get_MM_WeightVector(faces.native, &nativeLength, nullptr));
    Require(guestLength == nativeLength);
}

void CheckLargeCounts(const Faces& faces) {
    constexpr std::uint32_t count = 0x10003;
    std::vector<std::int64_t> guest(count, Untouched);
    std::vector<FT_Fixed> native(count, static_cast<FT_Fixed>(Untouched));
    Require(FT_Get_MM_Blend_Coordinates_nid_postfix(faces.guest, count, guest.data()) == FT_Get_MM_Blend_Coordinates(faces.native, count, native.data()));
    for (std::uint32_t index = 0; index < count; ++index) Require(guest[index] == native[index]);
    std::uint32_t guestLength = count;
    FT_UInt nativeLength = count;
    Require(FT_Get_MM_WeightVector_nid_postfix(faces.guest, &guestLength, guest.data()) == FT_Get_MM_WeightVector(faces.native, &nativeLength, native.data()));
    Require(guestLength == nativeLength);
    for (std::uint32_t index = 0; index < count; ++index) Require(guest[index] == native[index]);
}

void CheckSetters(const Faces& faces) {
    FreeType::GuestFaceRec& rec = *faces.guest;
    const FT_Face host = static_cast<FreeType::GuestFace*>(rec.internal)->host;
    FT_Fixed blend = 0x8000;
    if (FT_Set_MM_Blend_Coordinates(host, 1, &blend) == 0) {
        Require(FT_Set_MM_Blend_Coordinates(faces.native, 1, &blend) == 0);
        Require((host->face_flags & FT_FACE_FLAG_VARIATION) && !(rec.face_flags & FT_FACE_FLAG_VARIATION));
    }
    for (const std::uint32_t instance : {0u, 1u, 2u}) {
        rec.face_flags = 0;
        rec.ascender = 1234;
        const auto error = FT_Set_Named_Instance_nid_postfix(&rec, instance);
        Require(error == FT_Set_Named_Instance(faces.native, instance));
        Require(rec.face_flags == host->face_flags && host->face_flags == faces.native->face_flags);
        Require(rec.face_index == host->face_index && host->face_index == faces.native->face_index);
        Require(rec.ascender == host->ascender && (error != 0 || !(rec.face_flags & FT_FACE_FLAG_VARIATION)));
    }
    std::array<std::int64_t, 2> weights{0x8000, 0x8000};
    std::array<FT_Fixed, 2> native{0x8000, 0x8000};
    for (const std::uint32_t length : {0u, 2u}) {
        rec.ascender = 1234;
        Require(FT_Set_MM_WeightVector_nid_postfix(&rec, length, weights.data()) == FT_Set_MM_WeightVector(faces.native, length, native.data()));
        Require(rec.ascender == host->ascender && rec.face_flags == host->face_flags);
    }
    Require(FT_Set_MM_WeightVector_nid_postfix(&rec, UINT32_MAX, weights.data()) == FT_Err_Invalid_Argument);
    Require(FT_Set_MM_WeightVector_nid_postfix(&rec, 2, nullptr) == FT_Err_Invalid_Argument);
    Require(FT_Set_MM_WeightVector_nid_postfix(&rec, 0, nullptr) == FT_Set_MM_WeightVector(faces.native, 0, nullptr));
}

}

int main() {
    FreeType::GuestLibrary* guestLibrary = nullptr;
    FT_Library nativeLibrary = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&guestLibrary) == 0);
    Require(FT_Init_FreeType(&nativeLibrary) == 0);

    const auto variable = ReadFont("varc-reset-only.ttf");
    const auto fixed = ReadFont("varc-static-gvar.ttf");
    const Faces variableFaces = Open(guestLibrary, nativeLibrary, variable);
    const Faces fixedFaces = Open(guestLibrary, nativeLibrary, fixed);

    std::array<std::int64_t, 4> coords;
    coords.fill(Untouched);
    Require(FT_Get_MM_Blend_Coordinates_nid_postfix(variableFaces.guest, 3, coords.data()) == 0);
    Require(coords[0] == 0 && coords[1] == 0 && coords[2] == 0 && coords[3] == Untouched);

    for (const std::uint32_t count : {0u, 1u, 3u}) {
        CheckBlendCoordinates(variableFaces, count);
        CheckBlendCoordinates(fixedFaces, count);
    }
    for (const FT_UInt count : {0u, 2u}) {
        CheckWeightVector(variableFaces, count);
        CheckWeightVector(fixedFaces, count);
    }
    CheckLargeCounts(variableFaces);
    CheckLargeCounts(fixedFaces);

    CheckSetters(variableFaces);
    CheckSetters(fixedFaces);

    std::array<std::int64_t, 2> empty{Untouched, Untouched};
    std::uint32_t length = 2;
    Require(FT_Get_MM_Blend_Coordinates_nid_postfix(nullptr, 2, empty.data()) == FT_Err_Invalid_Face_Handle);
    Require(FT_Get_MM_WeightVector_nid_postfix(nullptr, &length, empty.data()) == FT_Err_Invalid_Face_Handle);
    Require(length == 2 && empty[0] == Untouched);
    Require(FT_Set_MM_WeightVector_nid_postfix(nullptr, 2, empty.data()) == FT_Err_Invalid_Face_Handle);
    Require(FT_Set_Named_Instance_nid_postfix(nullptr, 0) == FT_Err_Invalid_Face_Handle);

    Require(FT_Done_Face_nid_postfix(variableFaces.guest) == 0 && FT_Done_Face_nid_postfix(fixedFaces.guest) == 0);
    Require(FT_Done_Face(variableFaces.native) == 0 && FT_Done_Face(fixedFaces.native) == 0);
    Require(FT_Done_FreeType_nid_postfix(guestLibrary) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
    return 0;
}
