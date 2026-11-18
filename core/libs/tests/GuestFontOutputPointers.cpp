#include "FontFixture.hpp"
#include "prx/libSceFont/include/FontDriver.hpp"
#include "prx/libSceFontFt/include/Api.hpp"
#include "prx/libSceFont/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unordered_set>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

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

static void APS5_VABI Release(void*, void* pointer) {
    std::free(pointer);
}

static std::vector<unsigned char> BuildCharacterFont() {
    return FontFixture::BuildFont(3, {
        {"cmap", FontFixture::Words({0, 1, 3, 1, 0, 12, 4, 40, 0, 6, 4, 1, 2, 'A', 'V', 0xFFFF, 0, 'A', 'V', 0xFFFF, 1 - 'A', 2 - 'V', 1, 0, 0, 0})},
        {"kern", FontFixture::Words({0, 1, 0, 20, 1, 1, 6, 0, 0, 1, 2, -200})},
    });
}

static void Append32(std::vector<unsigned char>& bytes, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) bytes.push_back(static_cast<unsigned char>(value >> shift));
}

static std::uint32_t Read32(const std::vector<unsigned char>& bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(bytes[offset]) << 24) | (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) | bytes[offset + 3];
}

static void Write32(std::vector<unsigned char>& bytes, std::size_t offset, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) bytes[offset++] = static_cast<unsigned char>(value >> shift);
}

static std::vector<unsigned char> BuildCollection() {
    std::vector<unsigned char> first = FontFixture::BuildFont(3, {});
    std::vector<unsigned char> second = FontFixture::BuildFont(3, {});
    std::vector<unsigned char> result{'t', 't', 'c', 'f'};
    Append32(result, 0x10000);
    Append32(result, 2);
    Append32(result, 20);
    Append32(result, static_cast<std::uint32_t>(20 + first.size()));
    for (auto* face : {&first, &second}) {
        const std::uint32_t faceStart = static_cast<std::uint32_t>(result.size());
        const std::size_t tableCount = (static_cast<std::size_t>((*face)[4]) << 8) | (*face)[5];
        for (std::size_t i = 0; i < tableCount; ++i) {
            const std::size_t offset = 12 + 16 * i + 8;
            Write32(*face, offset, Read32(*face, offset) + faceStart);
        }
        result.insert(result.end(), face->begin(), face->end());
    }
    return result;
}

static std::unordered_set<void*> closePages;

static std::size_t PageSize() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return info.dwPageSize;
#else
    return static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#endif
}

static void* APS5_VABI CloseAllocate(void*, std::uint32_t size) {
    if (size != sizeof(Font::FontObj)) return std::malloc(size);
    void* page;
#ifdef _WIN32
    page = VirtualAlloc(nullptr, PageSize(), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    page = mmap(nullptr, PageSize(), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) page = nullptr;
#endif
    if (page) closePages.insert(page);
    return page;
}

static void APS5_VABI CloseRelease(void*, void* pointer) {
    if (closePages.erase(pointer) == 0) {
        std::free(pointer);
        return;
    }
#ifdef _WIN32
    VirtualFree(pointer, 0, MEM_RELEASE);
#else
    munmap(pointer, PageSize());
#endif
}

static bool SetWritable(void* page, bool writable) {
#ifdef _WIN32
    DWORD previous{};
    return VirtualProtect(page, PageSize(), writable ? PAGE_READWRITE : PAGE_READONLY, &previous) != 0;
#else
    return mprotect(page, PageSize(), writable ? PROT_READ | PROT_WRITE : PROT_READ) == 0;
#endif
}

static Font::FontObj* FontHead(FontHandle handle) {
    const auto* native = reinterpret_cast<const Font::FontHandleNative*>(handle);
    const auto* library = static_cast<const Font::FontLibNative*>(native->library);
    const auto* header = static_cast<const Font::FontCtxHeader*>(library->external_fonts_ctx);
    const auto* entries = static_cast<const Font::FontCtxEntry*>(header->base);
    return static_cast<Font::FontObj*>(entries[native->open_info.ctx_entry_index].obj_mode1);
}

static void CheckCloseUnlink() {
    const FontMemoryInterface iface{CloseAllocate, CloseRelease, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    FontLibrary library = nullptr;
    Require(sceFontCreateLibrary(&memory, sceFontSelectLibraryFt(0), &library) == SCE_FONT_OK);
    Require(sceFontSupportExternalFonts(library, 2, 0x52) == SCE_FONT_OK);
    const auto collection = BuildCollection();
    FontOpenDetail detail{};
    detail.unique_id = -1;
    FontHandle first = nullptr;
    FontHandle second = nullptr;
    Require(sceFontOpenFontMemory(library, collection.data(), static_cast<std::uint32_t>(collection.size()), &detail, &first) == SCE_FONT_OK);
    detail.subfont_index = 1;
    Require(sceFontOpenFontMemory(library, collection.data(), static_cast<std::uint32_t>(collection.size()), &detail, &second) == SCE_FONT_OK);
    Font::FontObj* newer = FontHead(second);
    Font::FontObj* older = newer->next;
    Require(older && newer->prev == nullptr && older->prev == newer && closePages.count(older) == 1);
    Require(SetWritable(older, false));
    errno = 71;
    Require(sceFontCloseFont(second) == SCE_FONT_ERROR_FATAL && errno == 71);
    Require(FontHead(second) == newer && newer->next == older && older->prev == newer && newer->ft_face != nullptr);
    Require(reinterpret_cast<const Font::FontHandleNative*>(second)->magic == Font::HANDLE_MAGIC);
    Require(SetWritable(older, true));
    Require(sceFontCloseFont(second) == SCE_FONT_OK);
    Require(FontHead(first) == older && older->prev == nullptr);
    Require(sceFontOpenFontMemory(library, collection.data(), static_cast<std::uint32_t>(collection.size()), &detail, &second) == SCE_FONT_OK);
    newer = FontHead(second);
    Require(newer->next == older && older->prev == newer && closePages.count(newer) == 1);
    Require(SetWritable(newer, false));
    errno = 71;
    Require(sceFontCloseFont(first) == SCE_FONT_ERROR_FATAL && errno == 71);
    Require(FontHead(first) == newer && newer->next == older && older->prev == newer && older->ft_face != nullptr);
    Require(reinterpret_cast<const Font::FontHandleNative*>(first)->magic == Font::HANDLE_MAGIC);
    Require(SetWritable(newer, true));
    Require(sceFontCloseFont(first) == SCE_FONT_OK);
    Require(FontHead(second) == newer && newer->next == nullptr);
    Require(sceFontCloseFont(second) == SCE_FONT_OK);
    Require(closePages.empty());
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK);
}

int main() {
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
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
    CheckCloseUnlink();
    return 0;
}
