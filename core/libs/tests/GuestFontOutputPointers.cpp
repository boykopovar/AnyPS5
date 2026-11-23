#include "FontFixture.hpp"
#include "prx/libSceFontFt/include/Api.hpp"
#include "prx/libSceFont/include/Api.hpp"
#include "prx/libSceFont/include/FontDriver.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using Font::FontLibNative;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font output pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void* APS5_VABI Allocate(void*, std::uint32_t size) {
    return std::malloc(size);
}

static void* trackedContext = nullptr;
static FontLibNative* trackedLibrary = nullptr;
static int contextReleases = 0;
static bool registryClearedBeforeRelease = false;

static void APS5_VABI Release(void*, void* pointer) {
    if (pointer == trackedContext && trackedContext) {
        ++contextReleases;
        registryClearedBeforeRelease = trackedLibrary->fontset_registry == nullptr;
    }
    std::free(pointer);
}

static void* AllocatePage(std::size_t& pageSize) {
#ifdef _WIN32
    SYSTEM_INFO systemInfo{};
    GetSystemInfo(&systemInfo);
    pageSize = systemInfo.dwPageSize;
    void* const page = VirtualAlloc(nullptr, pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(page != nullptr);
#else
    const long nativePageSize = sysconf(_SC_PAGESIZE);
    Require(nativePageSize > 0);
    pageSize = static_cast<std::size_t>(nativePageSize);
    void* const page = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
#endif
    return page;
}

static void ProtectPage(void* page, std::size_t pageSize, bool readOnly) {
#ifdef _WIN32
    DWORD previousProtection = 0;
    Require(VirtualProtect(page, pageSize, readOnly ? PAGE_READONLY : PAGE_READWRITE, &previousProtection) != 0);
#else
    Require(mprotect(page, pageSize, readOnly ? PROT_READ : PROT_READ | PROT_WRITE) == 0);
#endif
}

static void FreePage(void* page, std::size_t pageSize) {
#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, pageSize) == 0);
#endif
}

static void CheckLibraryTermTransaction(const FontMemory& memory) {
    const auto* driver = sceFontSelectLibraryFt(0);
    Require(driver != nullptr && driver->init != nullptr && driver->term != nullptr);
    std::size_t pageSize = 0;
    void* const page = AllocatePage(pageSize);
    auto* const native = static_cast<FontLibNative*>(page);
    native->iface = memory.iface;
    native->alloc_ctx = memory.mspace_handle;
    Require(driver->init(&memory, native) == SCE_FONT_OK && native->fontset_registry != nullptr);
    trackedLibrary = native;
    trackedContext = native->fontset_registry;
    contextReleases = 0;
    registryClearedBeforeRelease = false;

    ProtectPage(page, pageSize, true);
    errno = 71;
    Require(driver->term(native) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(errno == 71 && contextReleases == 0 && native->fontset_registry == trackedContext);
    ProtectPage(page, pageSize, false);
    Require(driver->term(native) == SCE_FONT_OK);
    Require(native->fontset_registry == nullptr && contextReleases == 1 && registryClearedBeforeRelease);
    trackedLibrary = nullptr;
    trackedContext = nullptr;
    FreePage(page, pageSize);
}

static FontLibNative* pageBackedLibrary = nullptr;
static std::size_t pageBackedSize = 0;

static void* APS5_VABI AllocatePageBacked(void*, std::uint32_t size) {
    if (size != sizeof(FontLibNative) || pageBackedLibrary) return std::malloc(size);
    pageBackedLibrary = static_cast<FontLibNative*>(AllocatePage(pageBackedSize));
    return pageBackedLibrary;
}

static void APS5_VABI ReleasePageBacked(void* ctx, void* pointer) {
    if (pointer && pointer == pageBackedLibrary) {
        FreePage(pointer, pageBackedSize);
        pageBackedLibrary = nullptr;
    } else {
        Release(ctx, pointer);
    }
}

static void CheckPublicLibraryTermTransaction() {
    const FontMemoryInterface iface{AllocatePageBacked, ReleasePageBacked, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    FontLibrary library = nullptr;
    Require(sceFontCreateLibrary(&memory, sceFontSelectLibraryFt(0), &library) == SCE_FONT_OK);
    Require(pageBackedLibrary == library && pageBackedLibrary->fontset_registry != nullptr);
    trackedLibrary = pageBackedLibrary;
    trackedContext = pageBackedLibrary->fontset_registry;
    contextReleases = 0;
    registryClearedBeforeRelease = false;
    ProtectPage(pageBackedLibrary, pageBackedSize, true);
    errno = 71;
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(errno == 71 && library == pageBackedLibrary && contextReleases == 0);
    Require(pageBackedLibrary->fontset_registry == trackedContext);
    ProtectPage(pageBackedLibrary, pageBackedSize, false);
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK && library == nullptr);
    Require(contextReleases == 1 && registryClearedBeforeRelease && pageBackedLibrary == nullptr);
    trackedLibrary = nullptr;
    trackedContext = nullptr;
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_OK);
}

static std::vector<unsigned char> BuildCharacterFont() {
    return FontFixture::BuildFont(3, {
        {"cmap", FontFixture::Words({0, 1, 3, 1, 0, 12, 4, 40, 0, 6, 4, 1, 2, 'A', 'V', 0xFFFF, 0, 'A', 'V', 0xFFFF, 1 - 'A', 2 - 'V', 1, 0, 0, 0})},
        {"kern", FontFixture::Words({0, 1, 0, 20, 1, 1, 6, 0, 0, 1, 2, -200})},
    });
}

int main() {
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    CheckLibraryTermTransaction(memory);
    CheckPublicLibraryTermTransaction();
    auto* const unmappedLibrary = reinterpret_cast<FontLibrary*>(0x10);
    errno = 71;
    Require(sceFontCreateLibraryWithEdition(&memory, sceFontSelectLibraryFt(0), 0, unmappedLibrary) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(errno == 71);
    errno = 71;
    Require(sceFontCreateLibrary(nullptr, sceFontSelectLibraryFt(0), unmappedLibrary) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(errno == 71);
    FontLibrary library = nullptr;
    Require(sceFontCreateLibrary(&memory, sceFontSelectLibraryFt(0), &library) == SCE_FONT_OK && library != nullptr);
    FontRenderer renderer = nullptr;
    Require(sceFontCreateRenderer(&memory, sceFontSelectRendererFt(0), &renderer) == SCE_FONT_OK && renderer != nullptr);

    auto* const unmappedCount = reinterpret_cast<std::uint32_t*>(0x10);

    errno = 71;
    Require(sceFontRendererGetOutlineBufferSize(renderer, unmappedCount) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(errno == 71);
    errno = 71;
    Require(sceFontRendererGetOutlineBufferSize(nullptr, unmappedCount) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(errno == 71);

    errno = 71;
    Require(sceFontGetPixelResolution(library, unmappedCount) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(errno == 71);
    errno = 71;
    Require(sceFontGetPixelResolution(nullptr, unmappedCount) == SCE_FONT_ERROR_INVALID_LIBRARY);
    Require(errno == 71);

    errno = 71;
    Require(sceFontGetLibrary(nullptr, unmappedLibrary) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(errno == 71);

    errno = 71;
    Require(sceFontGetFontGlyphsCount(nullptr, unmappedCount) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    Require(errno == 71);

    Require(sceFontRendererGetOutlineBufferSize(renderer, nullptr) == sceFontRendererGetOutlineBufferSize(renderer, unmappedCount));
    Require(sceFontRendererGetOutlineBufferSize(nullptr, nullptr) == sceFontRendererGetOutlineBufferSize(nullptr, unmappedCount));
    Require(sceFontGetPixelResolution(library, nullptr) == sceFontGetPixelResolution(library, unmappedCount));
    Require(sceFontGetPixelResolution(nullptr, nullptr) == sceFontGetPixelResolution(nullptr, unmappedCount));
    Require(sceFontGetLibrary(nullptr, nullptr) == sceFontGetLibrary(nullptr, unmappedLibrary));
    Require(sceFontGetFontGlyphsCount(nullptr, nullptr) == sceFontGetFontGlyphsCount(nullptr, unmappedCount));

    std::uint32_t size = 7;
    Require(sceFontRendererGetOutlineBufferSize(renderer, &size) == SCE_FONT_OK && size == 0x4000);
    std::uint32_t subPixelCount = 1;
    Require(sceFontGetPixelResolution(library, &subPixelCount) == SCE_FONT_OK && subPixelCount == 64);

    auto* const unmappedAttribute = reinterpret_cast<std::int32_t*>(0x10);
    Require(sceFontSupportExternalFonts(library, 1, 0x52) == SCE_FONT_OK);
    FontHandle badFont = nullptr;
    errno = 71;
    Require(sceFontOpenFontMemory(library, reinterpret_cast<const void*>(0x10), 64, nullptr, &badFont) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(errno == 71);
    Require(badFont == nullptr);
    const unsigned char malformedFont[64]{};
    Require(sceFontOpenFontMemory(library, malformedFont, sizeof(malformedFont), nullptr, &badFont) == SCE_FONT_ERROR_NO_SUPPORT_FORMAT);
    Require(badFont == nullptr);
    const std::vector<unsigned char> fontData = BuildCharacterFont();
    FontHandle font = nullptr;
    Require(sceFontOpenFontMemory(library, fontData.data(), static_cast<std::uint32_t>(fontData.size()), nullptr, &font) == SCE_FONT_OK);
    Require(sceFontSetScalePixel(font, 100.0f, 100.0f) == SCE_FONT_OK);
    FontGlyph glyph = nullptr;
    Require(sceFontGenerateCharGlyph(font, 'A', nullptr, &glyph) == SCE_FONT_OK && glyph != nullptr);

    errno = 71;
    Require(sceFontGlyphDefineAttribute(nullptr, 0x10, unmappedAttribute) == SCE_FONT_ERROR_INVALID_GLYPH);
    Require(errno == 71);
    errno = 71;
    Require(sceFontGlyphDefineAttribute(glyph, 0x20, unmappedAttribute) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(errno == 71);
    errno = 71;
    Require(sceFontGlyphDefineAttribute(glyph, 0x10, unmappedAttribute) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(errno == 71);
    Require(sceFontGlyphDefineAttribute(glyph, 0x11, nullptr) == SCE_FONT_OK);
    std::int32_t previous0 = 0, previous1 = 0, previous2 = 0;
    Require(sceFontGlyphDefineAttribute(glyph, 0x10, &previous0) == SCE_FONT_OK);
    Require(sceFontGlyphDefineAttribute(glyph, 0x11, &previous1) == SCE_FONT_OK && previous1 == 0x10);
    Require(sceFontGlyphDefineAttribute(glyph, 0x10, &previous2) == SCE_FONT_OK && previous2 == 0x11);

    Require(sceFontDeleteGlyph(&memory, &glyph) == SCE_FONT_OK && glyph == nullptr);
    Require(sceFontCloseFont(font) == SCE_FONT_OK);

    Require(sceFontDestroyRenderer(&renderer) == SCE_FONT_OK && renderer == nullptr);
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK && library == nullptr);
    return 0;
}
