#include "prx/libSceWkFontConfig/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT = static_cast<int>(0x813C0001);
constexpr std::uint32_t WkFontConfigAttrSize = 0x14;
constexpr std::uint32_t WkFontConfigFontIdCount = 0x96;

struct WkFontConfigAttrLimit {
    std::size_t offset;
    std::uint32_t maximum;
};

constexpr WkFontConfigAttrLimit WkFontConfigAttrLimits[] = {{0x4, 6}, {0x8, 9}, {0xC, 2}, {0x10, 2}};

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
        const auto* bytes = static_cast<const std::uint8_t*>(attr);
        std::uint32_t size = 0;
        if (!GuestCopyIn_nid_no_patch(&size, bytes, sizeof(size))) {
            NotImplemented_nid_no_patch(__func__);
            return 0;
        }
        if (size != WkFontConfigAttrSize) return SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT;
        for (const auto& limit : WkFontConfigAttrLimits) {
            std::uint32_t value = 0;
            if (!GuestCopyIn_nid_no_patch(&value, bytes + limit.offset, sizeof(value))) {
                NotImplemented_nid_no_patch(__func__);
                return 0;
            }
            if (value > limit.maximum) return SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT;
        }
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
