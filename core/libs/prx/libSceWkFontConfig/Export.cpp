#include "prx/libSceWkFontConfig/include/Api.hpp"
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT = static_cast<int>(0x813C0001);
constexpr int SCE_WK_FONT_CONFIG_ERROR_BUFFER_TOO_SMALL = static_cast<int>(0x813C0002);
constexpr std::size_t WkFontConfigFontNameOffset = 0x8;
constexpr std::size_t WkFontConfigFontAttrOffset = 0x10;
constexpr std::uint32_t WkFontConfigAttrSize = 0x14;
constexpr std::uint32_t WkFontConfigFontIdCount = 0x96;

struct WkFontConfigAttrLimit {
    std::size_t offset;
    std::uint32_t maximum;
};

constexpr WkFontConfigAttrLimit WkFontConfigAttrLimits[] = {{0x4, 6}, {0x8, 9}, {0xC, 2}, {0x10, 2}};

std::optional<std::string> ReadGuestString(const char* text) {
    constexpr std::size_t PageSize = 4096;
    std::string result;
    char chunk[256];
    for (;;) {
        const auto address = reinterpret_cast<std::uintptr_t>(text) + result.size();
        const std::size_t bytes = std::min<std::size_t>(sizeof(chunk), PageSize - address % PageSize);
        if (!GuestCopyIn_nid_no_patch(chunk, text + result.size(), bytes)) return std::nullopt;
        for (std::size_t index = 0; index < bytes; ++index) {
            if (chunk[index] == '\0') return result.append(chunk, index);
        }
        result.append(chunk, bytes);
    }
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
    unsigned char record[WkFontConfigAttrSize];
    if (font == nullptr || !GuestCopyIn_nid_no_patch(record, static_cast<const unsigned char*>(font) + WkFontConfigFontAttrOffset, sizeof(record)) ||
        !GuestCopyOut_nid_no_patch(attr, record, sizeof(record))) {
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }
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
    const char* namePointer = nullptr;
    if (!GuestCopyIn_nid_no_patch(&namePointer, static_cast<const unsigned char*>(font) + WkFontConfigFontNameOffset, sizeof(namePointer))) {
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }
    const std::optional<std::string> name = namePointer ? ReadGuestString(namePointer) : std::nullopt;
    if (!name) {
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }
    if (static_cast<std::uint32_t>(name->size()) + 1u > size) return SCE_WK_FONT_CONFIG_ERROR_BUFFER_TOO_SMALL;
    std::vector<char> copied(size, '\0');
    std::copy(name->begin(), name->end(), copied.begin());
    if (!GuestCopyOut_nid_no_patch(fontName, copied.data(), copied.size())) {
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }
    return 0;
}

int APS5_VABI sceWkFontConfigGetFontPath(const void* font, char* fontPath, std::uint32_t size) {
    if (font == nullptr || fontPath == nullptr) return SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT;
    static_cast<void>(size);
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
