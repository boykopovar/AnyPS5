#include "prx/libSceWkFontConfig/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT = static_cast<int>(0x813C0001);
constexpr std::uint32_t WkFontConfigAttrSize = 0x14;
constexpr std::uint32_t WkFontConfigFontIdCount = 0x96;

struct WkFontConfigAttr {
    std::uint32_t size;
    std::uint32_t field4;
    std::uint32_t field8;
    std::uint32_t fieldC;
    std::uint32_t field10;
};

static_assert(sizeof(WkFontConfigAttr) == WkFontConfigAttrSize);

bool AttrFieldsValid(const WkFontConfigAttr& attr) {
    return attr.size == WkFontConfigAttrSize && attr.field4 <= 6 && attr.field8 <= 9 && attr.fieldC <= 2 &&
           attr.field10 <= 2;
}

}

extern "C" {

APS5_DUMMY_FUN
int APS5_VABI sceWkFontConfigGetAttr(const void* font, void* attr) {
    if (attr == nullptr) return SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT;
    std::uint32_t size = 0;
    if (!GuestCopyIn_nid_no_patch(&size, attr, sizeof(size))) {
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }
    if (size != WkFontConfigAttrSize) return SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT;
    static_cast<void>(font);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceWkFontConfigGetFont(void* font, std::uint32_t fontId, const void* attr) {
    if (font == nullptr || fontId >= WkFontConfigFontIdCount) return SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT;
    if (attr != nullptr) {
        WkFontConfigAttr fields{};
        if (!GuestCopyIn_nid_no_patch(&fields, attr, sizeof(fields))) {
            NotImplemented_nid_no_patch(__func__);
            return 0;
        }
        if (!AttrFieldsValid(fields)) return SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceWkFontConfigGetFontFromFontName(void* font, const char* fontName) {
    if (font == nullptr || fontName == nullptr) return SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceWkFontConfigGetFontName(const void* font, char* fontName, std::uint32_t size) {
    if (font == nullptr || fontName == nullptr) return SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT;
    static_cast<void>(size);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceWkFontConfigGetFontPath(const void* font, char* fontPath, std::uint32_t size) {
    if (font == nullptr || fontPath == nullptr) return SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT;
    static_cast<void>(size);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
