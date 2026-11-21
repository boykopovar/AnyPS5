#include "prx/libSceWkFontConfig/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceSystemService/include/SystemService.hpp"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT = static_cast<int>(0x813C0001);
constexpr int SCE_WK_FONT_CONFIG_ERROR_BUFFER_TOO_SMALL = static_cast<int>(0x813C0002);
constexpr int SCE_WK_FONT_CONFIG_ERROR_NOT_FOUND = static_cast<int>(0x813C0003);
constexpr std::size_t WkFontConfigFontPathOffset = 0x0;
constexpr std::size_t WkFontConfigFontNameOffset = 0x8;
constexpr std::size_t WkFontConfigFontAttrOffset = 0x10;
constexpr std::uint32_t WkFontConfigAttrSize = 0x14;
constexpr std::uint32_t WkFontConfigFontIdCount = 0x96;
constexpr std::size_t WkFontConfigPathLimit = 0x400;
constexpr std::uint32_t WkFontConfigDefaultWeight = 4;
constexpr std::uint32_t WkFontConfigDefaultWeightAlternative = 5;
constexpr std::uint32_t WkFontConfigMonospaceFamily = 3;
constexpr std::uint32_t WkFontConfigItalicStyle = 1;
constexpr int SystemLanguageKorean = 9;
constexpr int SystemLanguageChineseSimplified = 11;
constexpr int SystemServiceParamIdLanguage = 1;

struct WkFontConfigAttr {
    std::uint32_t size;
    std::uint32_t family;
    std::uint32_t weight;
    std::uint32_t style;
    std::uint32_t width;
};

struct WkFontConfigFont {
    const char* path;
    const char* name;
    WkFontConfigAttr attr;
};

static_assert(offsetof(WkFontConfigFont, path) == WkFontConfigFontPathOffset);
static_assert(offsetof(WkFontConfigFont, name) == WkFontConfigFontNameOffset);
static_assert(offsetof(WkFontConfigFont, attr) == WkFontConfigFontAttrOffset);
static_assert(sizeof(WkFontConfigAttr) == WkFontConfigAttrSize);
static_assert(sizeof(WkFontConfigFont) == 0x28);

constexpr WkFontConfigFont DefaultFonts[] = {
    {"/common/font/DFHEI5-SONY.ttf", "DFHeiW5-A-SONY Regular", {0x14, 2, 4, 2, 2}},
};
constexpr WkFontConfigFont SansFonts[] = {
    {"/common/font/SST-Light.otf", "SST Light", {0x14, 2, 3, 2, 2}},
    {"/common/font/SST-Medium.otf", "SST Medium", {0x14, 2, 5, 2, 2}},
    {"/common/font/SST-Bold.otf", "SST Bold", {0x14, 2, 7, 2, 2}},
};
constexpr WkFontConfigFont ItalicFonts[] = {
    {"/common/font/SST-LightItalic.otf", "SST Light Italic", {0x14, 2, 3, 1, 2}},
    {"/common/font/SST-MediumItalic.otf", "SST Medium Italic", {0x14, 2, 5, 1, 2}},
    {"/common/font/SST-BoldItalic.otf", "SST Bold Italic", {0x14, 2, 7, 1, 2}},
};
constexpr WkFontConfigFont MonospaceFonts[] = {
    {"/common/font/SSTTypewriter-Roman.otf", "SST Typewriter Roman", {0x14, 3, 4, 2, 2}},
    {"/common/font/SSTTypewriter-Bd.otf", "SST Typewriter Bold", {0x14, 3, 7, 2, 2}},
};
constexpr WkFontConfigFont JapaneseFonts[] = {
    {"/common/font/SSTJpPro-Regular.otf", "SST Japanese Pro Regular", {0x14, 2, 4, 2, 2}},
    {"/common/font/SSTJpPro-Bold.otf", "SST Japanese Pro Bold", {0x14, 2, 7, 2, 2}},
};
constexpr WkFontConfigFont ArabicFonts[] = {
    {"/common/font2/SSTArabic-Light.otf", "SST Arabic Light", {0x14, 2, 3, 2, 2}},
    {"/common/font2/SSTArabic-Medium.otf", "SST Arabic Medium", {0x14, 2, 5, 2, 2}},
    {"/common/font2/SSTArabic-Bold.otf", "SST Arabic Bold", {0x14, 2, 7, 2, 2}},
};
constexpr WkFontConfigFont HangulFonts[] = {
    {"/common/font/SCEPS4Yoongd-Light.otf", "SCEPS4Yoongd-Light", {0x14, 2, 3, 2, 2}},
    {"/common/font/SCEPS4Yoongd-Medium.otf", "SCEPS4Yoongd-Medium", {0x14, 2, 5, 2, 2}},
    {"/common/font/SCEPS4Yoongd-Bold.otf", "SCEPS4Yoongd-Bold", {0x14, 2, 7, 2, 2}},
};

constexpr std::span<const WkFontConfigFont> NameSearchOrder[] = {SansFonts, ItalicFonts, MonospaceFonts, JapaneseFonts, ArabicFonts, DefaultFonts, HangulFonts};

std::atomic<int> hanUsesJapanese{-1};

[[noreturn]] void Fault(const char* function, const char* what) {
    throw std::runtime_error(std::string(function) + ": unreadable or unwritable " + what);
}

template <typename T>
T ReadGuest(const void* source, const char* function, const char* what) {
    T value;
    if (!GuestCopyIn_nid_no_patch(&value, source, sizeof(value))) Fault(function, what);
    return value;
}

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

std::string ReadFontString(const void* font, std::size_t offset, const char* function, const char* what) {
    const auto* text = ReadGuest<const char*>(static_cast<const unsigned char*>(font) + offset, function, "font");
    const std::optional<std::string> value = text ? ReadGuestString(text) : std::nullopt;
    if (!value) Fault(function, what);
    return *value;
}

class GuestText {
public:
    explicit GuestText(const char* text) : text(text) {}

    bool Equals(const char* other) {
        for (std::size_t index = 0;; ++index) {
            const char character = At(index);
            if (character != other[index]) return false;
            if (character == '\0') return true;
        }
    }

private:
    const char* text;
    std::string read;

    char At(std::size_t index) {
        while (read.size() <= index) read.push_back(ReadGuest<char>(text + read.size(), "sceWkFontConfigGetFontFromFontName", "font name"));
        return read[index];
    }
};

const WkFontConfigFont* ChooseWeight(std::span<const WkFontConfigFont> fonts, std::uint32_t weight) {
    const std::uint32_t wanted = weight == 0 ? WkFontConfigDefaultWeight : weight;
    const WkFontConfigFont* lighter = &fonts.front();
    for (const auto& font : fonts) {
        if (font.attr.weight == wanted || (wanted == WkFontConfigDefaultWeight && font.attr.weight == WkFontConfigDefaultWeightAlternative)) return &font;
        if (font.attr.weight <= wanted) lighter = &font;
    }
    return lighter;
}

bool HanUsesJapanese() {
    int state = hanUsesJapanese.load();
    if (state < 0) {
        int language = -1;
        const bool chineseOrKorean = sceSystemServiceParamGetInt(SystemServiceParamIdLanguage, &language) == 0 && language >= SystemLanguageKorean &&
                                     language <= SystemLanguageChineseSimplified;
        state = chineseOrKorean ? 0 : 1;
        hanUsesJapanese.store(state);
    }
    return state != 0;
}

const WkFontConfigFont* SelectFont(std::uint32_t fontId, const WkFontConfigAttr& attr) {
    switch (fontId) {
    case 0:
    case 20:
    case 22:
    case 55:
    case 108: return ChooseWeight(JapaneseFonts, attr.weight);
    case 1:
    case 2: return ChooseWeight(ArabicFonts, attr.weight);
    case 8:
    case 14:
    case 25:
        if (attr.family == WkFontConfigMonospaceFamily) return ChooseWeight(MonospaceFonts, attr.weight);
        return ChooseWeight(attr.style == WkFontConfigItalicStyle ? ItalicFonts : SansFonts, attr.weight);
    case 17: return HanUsesJapanese() ? ChooseWeight(JapaneseFonts, attr.weight) : &DefaultFonts[0];
    case 18:
    case 122: return ChooseWeight(HangulFonts, attr.weight);
    default: return &DefaultFonts[0];
    }
}

}

extern "C" {

APS5_DUMMY_FUN
int APS5_VABI sceWkFontConfigGetAttr(const void* font, void* attr) {
    if (attr == nullptr) return SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT;
    if (ReadGuest<std::uint32_t>(attr, __func__, "attribute block") != WkFontConfigAttrSize) return SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT;
    if (font == nullptr) Fault(__func__, "font");
    const auto* source = static_cast<const unsigned char*>(font) + WkFontConfigFontAttrOffset;
    auto* destination = static_cast<unsigned char*>(attr);
    std::uint32_t lastField;
    unsigned char prefix[WkFontConfigAttrSize - sizeof(lastField)];
    if (!GuestCopyIn_nid_no_patch(&lastField, source + sizeof(prefix), sizeof(lastField)) ||
        !GuestCopyOut_nid_no_patch(destination + sizeof(prefix), &lastField, sizeof(lastField)) ||
        !GuestCopyIn_nid_no_patch(prefix, source, sizeof(prefix)) ||
        !GuestCopyOut_nid_no_patch(destination, prefix, sizeof(prefix))) Fault(__func__, "font or attribute block");
    return 0;
}

int APS5_VABI sceWkFontConfigGetFont(void* font, std::uint32_t fontId, const void* attr) {
    if (font == nullptr || fontId >= WkFontConfigFontIdCount) return SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT;
    WkFontConfigAttr requested{WkFontConfigAttrSize, 0, 0, 0, 0};
    if (attr != nullptr) {
        const auto* bytes = static_cast<const unsigned char*>(attr);
        requested.size = ReadGuest<std::uint32_t>(bytes, __func__, "attribute block");
        if (requested.size != WkFontConfigAttrSize) return SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT;
        requested.family = ReadGuest<std::uint32_t>(bytes + offsetof(WkFontConfigAttr, family), __func__, "attribute block");
        if (requested.family > 6) return SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT;
        requested.weight = ReadGuest<std::uint32_t>(bytes + offsetof(WkFontConfigAttr, weight), __func__, "attribute block");
        if (requested.weight > 9) return SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT;
        requested.style = ReadGuest<std::uint32_t>(bytes + offsetof(WkFontConfigAttr, style), __func__, "attribute block");
        if (requested.style > 2) return SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT;
        requested.width = ReadGuest<std::uint32_t>(bytes + offsetof(WkFontConfigAttr, width), __func__, "attribute block");
        if (requested.width > 2) return SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT;
    }
    const WkFontConfigFont* selected = SelectFont(fontId, requested);
    if (!GuestCopyOut_nid_no_patch(font, &selected, sizeof(selected))) Fault(__func__, "font output");
    return 0;
}

int APS5_VABI sceWkFontConfigGetFontFromFontName(void* font, const char* fontName) {
    if (font == nullptr || fontName == nullptr) return SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT;
    GuestText name(fontName);
    for (const auto fonts : NameSearchOrder) {
        for (const auto& candidate : fonts) {
            if (!name.Equals(candidate.name)) continue;
            const WkFontConfigFont* found = &candidate;
            if (!GuestCopyOut_nid_no_patch(font, &found, sizeof(found))) Fault(__func__, "font output");
            return 0;
        }
    }
    return SCE_WK_FONT_CONFIG_ERROR_NOT_FOUND;
}

int APS5_VABI sceWkFontConfigGetFontName(const void* font, char* fontName, std::uint32_t size) {
    if (font == nullptr || fontName == nullptr) return SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT;
    const std::string name = ReadFontString(font, WkFontConfigFontNameOffset, __func__, "font name");
    if (static_cast<std::uint32_t>(name.size()) + 1u > size) return SCE_WK_FONT_CONFIG_ERROR_BUFFER_TOO_SMALL;
    std::size_t copied = std::min<std::size_t>(size, name.size());
    if (!GuestCopyOut_nid_no_patch(fontName, name.data(), copied)) Fault(__func__, "name buffer");
    static constexpr char padding[4096]{};
    while (copied < size) {
        const std::size_t bytes = std::min<std::size_t>(size - copied, sizeof(padding));
        if (!GuestCopyOut_nid_no_patch(fontName + copied, padding, bytes)) Fault(__func__, "name buffer");
        copied += bytes;
    }
    return 0;
}

int APS5_VABI sceWkFontConfigGetFontPath(const void* font, char* fontPath, std::uint32_t size) {
    if (font == nullptr || fontPath == nullptr) return SCE_WK_FONT_CONFIG_ERROR_INVALID_ARGUMENT;
    static const std::string sandbox = sceKernelGetFsSandboxRandomWord();
    const std::string path = ReadFontString(font, WkFontConfigFontPathOffset, __func__, "font path");
    if (path.size() >= WkFontConfigPathLimit || sandbox.size() > WkFontConfigPathLimit) return SCE_WK_FONT_CONFIG_ERROR_BUFFER_TOO_SMALL;
    const std::string full = "/" + sandbox + path;
    if (full.size() >= size) return SCE_WK_FONT_CONFIG_ERROR_BUFFER_TOO_SMALL;
    if (!GuestCopyOut_nid_no_patch(fontPath, full.c_str(), full.size() + 1)) Fault(__func__, "path buffer");
    return 0;
}

}
