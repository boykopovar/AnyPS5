#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "FontFixture.hpp"

#include FT_GX_VALIDATE_H
#include FT_OPENTYPE_VALIDATE_H
#include FT_MODULE_H

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType validation check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct Fixture {
    GuestLibrary* library = nullptr;
    GuestFaceRec* face = nullptr;
    std::vector<unsigned char> bytes;

    explicit Fixture(const std::map<std::string, std::vector<unsigned char>>& tables) : bytes(FontFixture::BuildFont(3, tables)) {
        Require(FT_Init_FreeType_nid_postfix(&library) == 0);
        Require(FT_New_Memory_Face_nid_postfix(library, bytes.data(), bytes.size(), 0, &face) == 0);
    }

    ~Fixture() {
        Require(FT_Done_Face_nid_postfix(face) == 0);
        Require(FT_Done_FreeType_nid_postfix(library) == 0);
    }
};

static void OpenType(Fixture& fixture, int expectedError, const std::vector<unsigned char>* expected) {
    std::array<const unsigned char*, 5> actual{};
    std::array<const unsigned char*, 5> native{};
    const FT_Face host = FaceOf(fixture.face)->host;
    const int result = FT_OpenType_Validate_nid_postfix(fixture.face, FT_VALIDATE_OT, &actual[0], &actual[1], &actual[2], &actual[3], &actual[4]);
    Require(result == FT_OpenType_Validate(host, FT_VALIDATE_OT, &native[0], &native[1], &native[2], &native[3], &native[4]));
    Require(expectedError < 0 ? result != 0 : result == expectedError);
    if (result != 0) return;
    for (std::size_t index = 0; index < actual.size(); ++index) {
        Require((actual[index] == nullptr) == (native[index] == nullptr));
        if (actual[index]) {
            Require(index == 1 && expected);
            Require(std::memcmp(actual[index], expected->data(), expected->size()) == 0);
            Require(std::memcmp(actual[index], native[index], expected->size()) == 0);
        }
        FT_OpenType_Free_nid_postfix(fixture.face, actual[index]);
        FT_OpenType_Free(host, native[index]);
    }
}

static void Gx(Fixture& fixture, int expectedError, const std::vector<unsigned char>* expected) {
    const FT_Face host = FaceOf(fixture.face)->host;
    for (const std::uint32_t count : {0u, 1u, 2u, 10u, 12u}) {
        std::array<const unsigned char*, 13> actual{};
        std::array<const unsigned char*, 13> native{};
        actual[count] = native[count] = reinterpret_cast<const unsigned char*>(1);
        const int result = FT_TrueTypeGX_Validate_nid_postfix(fixture.face, FT_VALIDATE_GX, actual.data(), count);
        Require(result == FT_TrueTypeGX_Validate(host, FT_VALIDATE_GX, native.data(), count));
        Require((expectedError < 0 && count > FT_VALIDATE_mort_INDEX) ? result != 0 : result == 0);
        Require(actual[count] == reinterpret_cast<const unsigned char*>(1));
        if (result != 0) continue;
        for (std::size_t index = 0; index < count; ++index) {
            Require((actual[index] == nullptr) == (native[index] == nullptr));
            if (actual[index]) {
                Require(index == FT_VALIDATE_mort_INDEX && expected);
                Require(std::memcmp(actual[index], expected->data(), expected->size()) == 0);
                Require(std::memcmp(actual[index], native[index], expected->size()) == 0);
            }
            FT_TrueTypeGX_Free_nid_postfix(fixture.face, actual[index]);
            FT_TrueTypeGX_Free(host, native[index]);
        }
    }
}

static void Kern(Fixture& fixture, int expectedError, const std::vector<unsigned char>* expected) {
    const FT_Face host = FaceOf(fixture.face)->host;
    for (const std::uint32_t flags : {0u, static_cast<std::uint32_t>(FT_VALIDATE_MS), static_cast<std::uint32_t>(FT_VALIDATE_CKERN)}) {
        const unsigned char* actual = nullptr;
        const unsigned char* native = nullptr;
        const int result = FT_ClassicKern_Validate_nid_postfix(fixture.face, flags, &actual);
        Require(result == FT_ClassicKern_Validate(host, flags, &native));
        Require(expectedError < 0 ? result != 0 : result == expectedError);
        if (result != 0) continue;
        Require((actual == nullptr) == (native == nullptr));
        if (actual) {
            Require(expected);
            Require(std::memcmp(actual, expected->data(), expected->size()) == 0);
            Require(std::memcmp(actual, native, expected->size()) == 0);
        }
        FT_ClassicKern_Free_nid_postfix(fixture.face, actual);
        FT_ClassicKern_Free(host, native);
    }
}

static void Invalid(Fixture& fixture) {
    std::array<const unsigned char*, 5> output{};
    GuestFaceRec empty{};
    for (GuestFaceRec* face : {static_cast<GuestFaceRec*>(nullptr), &empty}) {
        Require(FT_OpenType_Validate_nid_postfix(face, 0, nullptr, nullptr, nullptr, nullptr, nullptr) == FT_Err_Invalid_Face_Handle);
        Require(FT_TrueTypeGX_Validate_nid_postfix(face, 0, nullptr, 0) == FT_Err_Invalid_Face_Handle);
        Require(FT_ClassicKern_Validate_nid_postfix(face, 0, nullptr) == FT_Err_Invalid_Face_Handle);
        FT_OpenType_Free_nid_postfix(face, nullptr);
        FT_TrueTypeGX_Free_nid_postfix(face, nullptr);
        FT_ClassicKern_Free_nid_postfix(face, nullptr);
    }
    for (std::size_t missing = 0; missing < output.size(); ++missing) {
        std::array<const unsigned char**, 5> args{&output[0], &output[1], &output[2], &output[3], &output[4]};
        args[missing] = nullptr;
        Require(FT_OpenType_Validate_nid_postfix(fixture.face, 0, args[0], args[1], args[2], args[3], args[4]) == FT_Err_Invalid_Argument);
    }
    Require(FT_TrueTypeGX_Validate_nid_postfix(fixture.face, 0, nullptr, 0) == FT_Err_Invalid_Argument);
    Require(FT_ClassicKern_Validate_nid_postfix(fixture.face, 0, nullptr) == FT_Err_Invalid_Argument);
}

int main() {
    Fixture missing({});
    const bool hasOpenType = FT_Get_Module(missing.library->host, "otvalid") != nullptr;
    const bool hasGx = FT_Get_Module(missing.library->host, "gxvalid") != nullptr;
    Require(hasOpenType || !ANYPS5_FREETYPE_BUNDLED);
    Require(hasGx || !ANYPS5_FREETYPE_BUNDLED);
    Invalid(missing);
    const auto gdef = FontFixture::Words({1, 0, 0, 0, 0, 0});
    const auto mort = FontFixture::Words({1, 0, 0, 0});
    const auto kern = FontFixture::Words({0, 1, 0, 20, 1, 1, 6, 0, 0, 1, 2, -25});
    Fixture validOpenType({{"GDEF", gdef}});
    Fixture validGx({{"mort", mort}});
    Fixture validKern({{"kern", kern}});
    Fixture invalid({{"GDEF", {0}}, {"mort", {0}}, {"kern", {0}}});
    if (hasOpenType) {
        OpenType(missing, 0, nullptr);
        OpenType(validOpenType, 0, &gdef);
        OpenType(invalid, -1, nullptr);
    } else {
        OpenType(missing, FT_Err_Unimplemented_Feature, nullptr);
    }
    if (hasGx) {
        Gx(missing, 0, nullptr);
        Kern(missing, 0, nullptr);
        Gx(validGx, 0, &mort);
        Kern(validKern, 0, &kern);
        Gx(invalid, -1, nullptr);
        Kern(invalid, -1, nullptr);
    } else {
        Require(FT_TrueTypeGX_Validate_nid_postfix(missing.face, 0, nullptr, 0) == FT_Err_Invalid_Argument);
        std::array<const unsigned char*, 10> tables{};
        Require(FT_TrueTypeGX_Validate_nid_postfix(missing.face, FT_VALIDATE_GX, tables.data(), tables.size()) == FT_Err_Unimplemented_Feature);
        Kern(missing, FT_Err_Unimplemented_Feature, nullptr);
    }
}
