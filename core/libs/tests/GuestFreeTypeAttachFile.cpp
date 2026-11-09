#include "FontFixture.hpp"
#include "Type1Fixture.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType attach file check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct FacePair {
    std::vector<unsigned char> bytes;
    GuestLibrary library{};
    FT_Library native = nullptr;
    GuestFace guest;
    FT_Face oracle = nullptr;

    explicit FacePair(std::vector<unsigned char> font) : bytes(std::move(font)) {
        Require(FT_Init_FreeType(&library.host) == 0 && FT_Init_FreeType(&native) == 0);
        guest.library = &library;
        Require(FT_New_Memory_Face(library.host, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &guest.host) == 0);
        Require(FT_New_Memory_Face(native, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &oracle) == 0);
        SyncFace(guest);
    }

    ~FacePair() {
        Require(FT_Done_Face(guest.host) == 0 && FT_Done_Face(oracle) == 0);
        Require(FT_Done_FreeType(library.host) == 0 && FT_Done_FreeType(native) == 0);
    }

    void KerningMatches(std::uint32_t left, std::uint32_t right, FT_Pos expected) const {
        FT_Vector actual{7, 7};
        FT_Vector reference{7, 7};
        Require(FT_Get_Kerning(guest.host, left, right, FT_KERNING_UNSCALED, &actual) == FT_Get_Kerning(oracle, left, right, FT_KERNING_UNSCALED, &reference));
        Require(actual.x == reference.x && actual.y == reference.y && reference.x == expected);
    }
};

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-freetype-attach-" + std::to_string(std::random_device{}()));
    Require(std::filesystem::create_directory(root));
    std::filesystem::create_directory(root / "app0");
    const std::string afm = Type1Fixture::Afm();
    {
        std::ofstream file(root / "app0" / "fixture.afm", std::ios::binary);
        file.write(afm.data(), static_cast<std::streamsize>(afm.size()));
        Require(static_cast<bool>(file));
    }
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(root);
    const std::string nativePath = (root / "app0" / "fixture.afm").string();
    {
        FacePair pair(Type1Fixture::Font());
        const std::uint32_t glyph = FT_Get_Char_Index(pair.oracle, 'A');
        Require(glyph != 0 && glyph == FT_Get_Char_Index(pair.guest.host, 'A'));
        pair.KerningMatches(glyph, glyph, 0);
        Require(FT_Attach_File_nid_postfix(&pair.guest.rec, "/app0/fixture.afm") == FT_Attach_File(pair.oracle, nativePath.c_str()));
        pair.KerningMatches(glyph, glyph, -50);
        Require(FT_Attach_File_nid_postfix(&pair.guest.rec, "/app0/fixture.afm") == FT_Attach_File(pair.oracle, nativePath.c_str()));
        pair.KerningMatches(glyph, glyph, -50);
        Require(FT_Attach_File_nid_postfix(&pair.guest.rec, "/app0/missing.afm") == FT_Err_Cannot_Open_Resource);
        Require(FT_Attach_File(pair.oracle, (root / "app0" / "missing.afm").string().c_str()) == FT_Err_Cannot_Open_Resource);
        Require(FT_Attach_File_nid_postfix(&pair.guest.rec, nullptr) == FT_Attach_File(pair.oracle, nullptr));
    }
    {
        FacePair pair(FontFixture::BuildFont(3, {}));
        Require(FT_Attach_File_nid_postfix(&pair.guest.rec, "/app0/fixture.afm") == FT_Attach_File(pair.oracle, nativePath.c_str()));
    }
    Require(FT_Attach_File_nid_postfix(nullptr, "/app0/fixture.afm") == FT_Attach_File(nullptr, nativePath.c_str()));
    GuestFaceRec empty{};
    Require(FT_Attach_File_nid_postfix(&empty, "/app0/fixture.afm") == FT_Attach_File(nullptr, nativePath.c_str()));
    std::filesystem::current_path(previous);
    std::filesystem::remove_all(root);
}
