#include "prx/libfreetype/libfreetype.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/Api.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_PARAMETER_TAGS_H
#include <freetype/internal/ftobjs.h>
#include <freetype/internal/tttypes.h>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

std::vector<unsigned char> ReadFont(const char* name) {
    std::ifstream file(std::string(FREETYPE_TEST_FONTS) + "/" + name, std::ios::binary);
    std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(file), {}};
    Require(!bytes.empty());
    return bytes;
}

std::int32_t SetProperty(FreeType::GuestFaceRec* face, std::uint64_t tag, void* data) {
    LegacyFreeType::GuestParameter property{tag, data};
    return FT_Face_Properties_nid_postfix(face, 1, &property);
}

}

int main() {
    FreeType::GuestLibrary* library = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&library) == 0);
    FT_UInt interpreterVersion = 40;
    Require(FT_Property_Set_nid_postfix(library, "truetype", "interpreter-version", &interpreterVersion) == 0);
    Require(FT_Property_Get_nid_postfix(library, "truetype", "interpreter-version", &interpreterVersion) == 0);
    Require(interpreterVersion == 40);
#ifdef _WIN32
    Require(_putenv_s("FREETYPE_PROPERTIES", "truetype:interpreter-version=38") == 0);
#else
    Require(setenv("FREETYPE_PROPERTIES", "truetype:interpreter-version=38", 1) == 0);
#endif
    Require(setenv_nid_postfix("FREETYPE_PROPERTIES", "truetype:interpreter-version=35", 1) == 0);
    FT_Set_Default_Properties_nid_postfix(library);
    Require(FT_Property_Get_nid_postfix(library, "truetype", "interpreter-version", &interpreterVersion) == 0);
    Require(interpreterVersion == 35);
    Require(unsetenv_nid_postfix("FREETYPE_PROPERTIES") == 0);
    interpreterVersion = 40;
    Require(FT_Property_Set_nid_postfix(library, "truetype", "interpreter-version", &interpreterVersion) == 0);
    FT_Set_Default_Properties_nid_postfix(library);
    Require(FT_Property_Get_nid_postfix(library, "truetype", "interpreter-version", &interpreterVersion) == 0);
    Require(interpreterVersion == 40);
#ifdef _WIN32
    Require(_putenv_s("FREETYPE_PROPERTIES", "") == 0);
#else
    Require(unsetenv("FREETYPE_PROPERTIES") == 0);
#endif
    const auto bytes = ReadFont("varc-reset-only.ttf");
    FreeType::GuestFaceRec* face = nullptr;
    Require(FT_New_Memory_Face_nid_postfix(library, bytes.data(), static_cast<std::int64_t>(bytes.size()), 0, &face) == 0);
    const FT_Face host = static_cast<FreeType::GuestFace*>(face->internal)->host;

    FT_Bool darkening = 1;
    FT_Int32 seed = 1234;
    LegacyFreeType::GuestParameter properties[]{{FT_PARAM_TAG_STEM_DARKENING, &darkening}, {FT_PARAM_TAG_RANDOM_SEED, &seed}};
    Require(FT_Face_Properties_nid_postfix(face, 2, properties) == 0);
    Require(host->internal->no_stem_darkening == 0 && host->internal->random_seed == 1234);

    darkening = 0;
    seed = -5;
    Require(FT_Face_Properties_nid_postfix(face, 2, properties) == 0);
    Require(host->internal->no_stem_darkening == 1 && host->internal->random_seed == 0);

    Require(SetProperty(face, FT_PARAM_TAG_STEM_DARKENING, nullptr) == 0 && SetProperty(face, FT_PARAM_TAG_RANDOM_SEED, nullptr) == 0);
    Require(host->internal->no_stem_darkening == -1 && host->internal->random_seed == -1);

    seed = 7;
    Require(SetProperty(face, FT_PARAM_TAG_LCD_FILTER_WEIGHTS, &seed) == FT_Err_Unimplemented_Feature);
    Require(SetProperty(face, FT_MAKE_TAG('a', 'b', 'c', 'd'), &seed) == FT_Err_Invalid_Argument);
    Require(SetProperty(face, (std::uint64_t{1} << 32) | FT_PARAM_TAG_RANDOM_SEED, &seed) == FT_Err_Invalid_Argument);
    Require(host->internal->random_seed == -1);
    LegacyFreeType::GuestParameter stopping[]{{FT_PARAM_TAG_RANDOM_SEED, &seed}, {FT_MAKE_TAG('a', 'b', 'c', 'd'), &seed}};
    Require(FT_Face_Properties_nid_postfix(face, UINT32_MAX, stopping) == FT_Err_Invalid_Argument);
    Require(host->internal->random_seed == 7);
    Require(FT_Face_Properties_nid_postfix(face, 1, nullptr) == FT_Err_Invalid_Argument);
    Require(FT_Face_Properties_nid_postfix(face, 0, nullptr) == 0);
    Require(FT_Face_Properties_nid_postfix(nullptr, 2, properties) == FT_Err_Invalid_Face_Handle);

    const auto* sfnt = reinterpret_cast<TT_Face>(host);
    Require(!sfnt->have_foreground_color);
    Require(FT_Palette_Set_Foreground_Color_nid_postfix(face, {0x10, 0x20, 0x30, 0x40}) == 0);
    Require(sfnt->have_foreground_color && sfnt->foreground_color.blue == 0x10 && sfnt->foreground_color.green == 0x20);
    Require(sfnt->foreground_color.red == 0x30 && sfnt->foreground_color.alpha == 0x40);
    Require(FT_Palette_Set_Foreground_Color_nid_postfix(nullptr, {}) == FT_Err_Invalid_Face_Handle);

    Require(FT_Done_Face_nid_postfix(face) == 0 && FT_Done_FreeType_nid_postfix(library) == 0);
    return 0;
}
