#include "FontFixture.hpp"
#include "prx/libSceFont/include/Api.hpp"
#include "prx/libSceFont/include/FontTypes.hpp"
#include "prx/libSceFontFt/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#ifdef _WIN32
#include "prx/libc/include/GuestTimeZoneName.hpp"
#include <string>
#include <windows.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font rebind renderer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

#ifdef _WIN32
static void VerifyGuestTimeZoneNameToUtf8() {
    Require(GuestTimeZoneNameToUtf8(nullptr, CP_UTF8).empty());
    Require(GuestTimeZoneNameToUtf8("", CP_UTF8).empty());
    Require(GuestTimeZoneNameToUtf8("", 1252u).empty());
    Require(GuestTimeZoneNameToUtf8("UTC", CP_UTF8) == "UTC");
    Require(GuestTimeZoneNameToUtf8("UTC", 1252u) == "UTC");
    Require(GuestTimeZoneNameToUtf8("\xE9", 1252u) == "\xC3\xA9");
    Require(GuestTimeZoneNameToUtf8("UTC", CP_UTF7) == "UTC");
    Require(GuestTimeZoneNameToUtf8("+AOk-", CP_UTF7) == "\xC3\xA9");

    const std::string invalidByte("\xFF", 1);
    const std::string invalidByteResult = GuestTimeZoneNameToUtf8(invalidByte.c_str(), CP_UTF8);
    Require(invalidByteResult == invalidByte);
    Require(invalidByteResult != "\xEF\xBF\xBD");

    const std::string overlongSequence("\xC0\xAF", 2);
    const std::string overlongResult = GuestTimeZoneNameToUtf8(overlongSequence.c_str(), CP_UTF8);
    Require(overlongResult == overlongSequence);
    Require(overlongResult != "\xEF\xBF\xBD\xEF\xBF\xBD");

    SetLastError(12345u);
    Require(GuestTimeZoneNameToUtf8("UTC", 0xFFFFFFFFu) == "UTC");
    Require(GetLastError() == 12345u);

    SetLastError(12345u);
    Require(GuestTimeZoneNameToUtf8(invalidByte.c_str(), CP_UTF8) == invalidByte);
    Require(GetLastError() == 12345u);

    SetLastError(12345u);
    Require(GuestTimeZoneNameToUtf8("+AOk-", CP_UTF7) == "\xC3\xA9");
    Require(GetLastError() == 12345u);
}
#endif

static int allocations = 0;
static void* APS5_VABI Allocate(void*, std::uint32_t size) {
    ++allocations;
    return std::malloc(size);
}
static void APS5_VABI Release(void*, void* pointer) {
    if (pointer) --allocations;
    std::free(pointer);
}

int main() {
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    FontLibrary library = nullptr;
    Require(sceFontCreateLibrary(&memory, sceFontSelectLibraryFt(0), &library) == SCE_FONT_OK);
    Require(sceFontSupportExternalFonts(library, 1, 0x52) == SCE_FONT_OK);
    const std::vector<unsigned char> fontData = FontFixture::BuildFont(1, {});
    FontHandle font = nullptr;
    FontHandle secondFont = nullptr;
    Require(sceFontOpenFontMemory(library, fontData.data(), static_cast<std::uint32_t>(fontData.size()), nullptr, &font) == SCE_FONT_OK);
    Require(sceFontOpenFontMemory(library, fontData.data(), static_cast<std::uint32_t>(fontData.size()), nullptr, &secondFont) == SCE_FONT_OK);

    Require(sceFontRebindRenderer(nullptr) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    Require(sceFontRebindRenderer(nullptr) == static_cast<int>(0x80460005));
    alignas(16) static unsigned char zeroedBlock[256] = {};
    Require(sceFontRebindRenderer(reinterpret_cast<FontHandle>(zeroedBlock)) == static_cast<int>(0x80460005));
    alignas(16) static unsigned char notAFont[16384] = {};
    Require(sceFontRebindRenderer(reinterpret_cast<FontHandle>(notAFont)) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);

    Require(sceFontRebindRenderer(font) == SCE_FONT_ERROR_NOT_BOUND_RENDERER);
    Require(sceFontRebindRenderer(secondFont) == SCE_FONT_ERROR_NOT_BOUND_RENDERER);

    FontRenderer renderer = nullptr;
    Require(sceFontCreateRenderer(&memory, sceFontSelectRendererFt(0), &renderer) == SCE_FONT_OK);
    Require(sceFontBindRenderer(font, renderer) == SCE_FONT_OK);
    Require(sceFontBindRenderer(secondFont, renderer) == SCE_FONT_OK);
    Require(sceFontRebindRenderer(font) == SCE_FONT_OK);
    Require(sceFontRebindRenderer(font) == SCE_FONT_OK);
    Require(sceFontRebindRenderer(secondFont) == SCE_FONT_OK);

    Require(sceFontUnbindRenderer(font) == SCE_FONT_OK);
    Require(sceFontRebindRenderer(font) == SCE_FONT_ERROR_NOT_BOUND_RENDERER);
    Require(sceFontRebindRenderer(secondFont) == SCE_FONT_OK);
    Require(sceFontUnbindRenderer(secondFont) == SCE_FONT_OK);
    Require(sceFontRebindRenderer(secondFont) == SCE_FONT_ERROR_NOT_BOUND_RENDERER);

    Require(sceFontDestroyRenderer(&renderer) == SCE_FONT_OK);
    Require(sceFontCloseFont(secondFont) == SCE_FONT_OK);
    Require(sceFontCloseFont(font) == SCE_FONT_OK);
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK);
    Require(allocations == 0);
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_OK);
#ifdef _WIN32
    VerifyGuestTimeZoneNameToUtf8();
#endif
}
