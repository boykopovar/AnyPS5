#include "FontFixture.hpp"
#include "prx/libSceFontFt/include/Api.hpp"
#include "prx/libSceFont/include/Api.hpp"
#include "prx/libSceFont/include/FontDriver.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string_view>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
#ifdef __linux__
#include <sys/mman.h>
#include <unistd.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font output pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static int liveAllocations = 0;
#ifdef __linux__
static void* lockOnAllocate = nullptr;
#endif

static void* APS5_VABI Allocate(void*, std::uint32_t size) {
#ifdef __linux__
    if (lockOnAllocate) {
        Require(mprotect(lockOnAllocate, static_cast<std::size_t>(sysconf(_SC_PAGESIZE)), PROT_READ) == 0);
        lockOnAllocate = nullptr;
    }
#endif
    void* block = std::malloc(size);
    if (block) ++liveAllocations;
    return block;
}

static void APS5_VABI Release(void*, void* pointer) {
    if (pointer) --liveAllocations;
    std::free(pointer);
}

static const Font::SysDriver* DriverWithUnreadableTerm(const Font::SysDriver* driver) {
    constexpr std::size_t readable = offsetof(Font::SysDriver, term);
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t page = info.dwPageSize;
    auto* base = static_cast<unsigned char*>(VirtualAlloc(nullptr, page * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Require(base != nullptr);
    DWORD previous = 0;
    Require(VirtualProtect(base + page, page, PAGE_NOACCESS, &previous) != 0);
#else
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    void* mapped = mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapped != MAP_FAILED);
    auto* base = static_cast<unsigned char*>(mapped);
    Require(mprotect(base + page, page, PROT_NONE) == 0);
#endif
    unsigned char* copy = base + page - readable;
    std::memcpy(copy, driver, readable);
    return reinterpret_cast<const Font::SysDriver*>(copy);
}

static std::size_t PageSize() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return info.dwPageSize;
#else
    return static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#endif
}

static const Font::SysDriver* DriverOnOwnPage(const Font::SysDriver* driver) {
    Require(sizeof(Font::SysDriver) <= PageSize());
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, PageSize(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Require(page != nullptr);
#else
    void* page = mmap(nullptr, PageSize(), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
#endif
    std::memcpy(page, driver, sizeof(Font::SysDriver));
    return static_cast<const Font::SysDriver*>(page);
}

static void SetDriverReadable(const Font::SysDriver* driver, bool readable) {
    void* page = const_cast<Font::SysDriver*>(driver);
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, PageSize(), readable ? PAGE_READWRITE : PAGE_NOACCESS, &previous) != 0);
#else
    Require(mprotect(page, PageSize(), readable ? PROT_READ | PROT_WRITE : PROT_NONE) == 0);
#endif
}

static void ReleaseDriverPage(const Font::SysDriver* driver) {
    void* page = const_cast<Font::SysDriver*>(driver);
#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, PageSize()) == 0);
#endif
}

static std::vector<unsigned char> BuildCharacterFont() {
    return FontFixture::BuildFont(3, {
        {"cmap", FontFixture::Words({0, 1, 3, 1, 0, 12, 4, 40, 0, 6, 4, 1, 2, 'A', 'V', 0xFFFF, 0, 'A', 'V', 0xFFFF, 1 - 'A', 2 - 'V', 1, 0, 0, 0})},
        {"kern", FontFixture::Words({0, 1, 0, 20, 1, 1, 6, 0, 0, 1, 2, -200})},
    });
}

static int CheckWatched() {
    using namespace GuestArena;
    constexpr std::size_t bytes = 65536;
#ifndef _WIN32
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
#ifdef __APPLE__
        std::abort();
#else
        std::puts("Page write watch is unavailable");
        return 77;
#endif
    }
#else
    Require(GuestArenaWriteWatched_nid_postfix());
#endif
    auto* pages = static_cast<std::byte*>(GuestArenaAllocate_nid_postfix(bytes, bytes));
    Require(pages != nullptr);
#ifdef _WIN32
    const auto section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE, 0, bytes, nullptr);
    Require(section != nullptr);
    GuestArenaMap_nid_postfix(pages, bytes, section, 0, PAGE_READWRITE);
    CloseHandle(section);
#else
    GuestArenaCommit_nid_postfix(pages, bytes, 0x04, bytes);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(pages, bytes);
#endif
    const auto arm = [&] {
#ifdef _WIN32
        std::array<void*, 32> written{};
        auto count = written.size();
        Require(GuestArenaCollectWrites_nid_postfix(reinterpret_cast<std::uintptr_t>(pages), bytes, written.data(), &count, true));
        for (const std::size_t offset : {0u, 16384u, 32768u, 49152u}) {
            MEMORY_BASIC_INFORMATION info{};
            Require(VirtualQuery(pages + offset, &info, sizeof(info)) == sizeof(info) && info.Protect == PAGE_READONLY);
        }
#else
        bool written = false;
        Require(GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(reinterpret_cast<std::uintptr_t>(pages), bytes,
            [](void* context, std::uintptr_t, std::uintptr_t) { *static_cast<bool*>(context) = true; }, &written));
#endif
    };
    auto* const memory = new (pages) FontMemory{};
    auto* const library = new (pages + 16384) FontLibrary{};
    auto* const renderer = new (pages + 16384 + 64) FontRenderer{};
    auto* const font = new (pages + 32768) FontHandle{};
    auto* const glyph = new (pages + 49152) FontGlyph{};
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    Require(sceFontMemoryInit(memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    Require(sceFontCreateLibrary(memory, sceFontSelectLibraryFt(0), library) == SCE_FONT_OK && *library != nullptr);
    Require(sceFontCreateRenderer(memory, sceFontSelectRendererFt(0), renderer) == SCE_FONT_OK && *renderer != nullptr);
    Require(sceFontSupportExternalFonts(*library, 1, 0x52) == SCE_FONT_OK);
    const std::vector<unsigned char> fontData = BuildCharacterFont();
    arm();
    Require(sceFontOpenFontMemory(*library, fontData.data(), static_cast<std::uint32_t>(fontData.size()), nullptr, font) == SCE_FONT_OK && *font != nullptr);
    Require(sceFontSetScalePixel(*font, 100.0f, 100.0f) == SCE_FONT_OK);
    arm();
    Require(sceFontGenerateCharGlyph(*font, 'A', nullptr, glyph) == SCE_FONT_OK && *glyph != nullptr);
    arm();
    Require(sceFontDeleteGlyph(memory, glyph) == SCE_FONT_OK && *glyph == nullptr);
    Require(sceFontCloseFont(*font) == SCE_FONT_OK);
    arm();
    Require(sceFontDestroyRenderer(renderer) == SCE_FONT_OK && *renderer == nullptr);
    arm();
    Require(sceFontDestroyLibrary(library) == SCE_FONT_OK && *library == nullptr);
    arm();
    Require(sceFontMemoryTerm(memory) == SCE_FONT_OK);
#ifndef _WIN32
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(pages, bytes);
#endif
    GuestArenaReset_nid_postfix(pages, bytes);
    GuestArenaRelease_nid_postfix(pages, bytes);
    std::puts("font outputs on watched guest pages passed");
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--watched") return CheckWatched();
    if (argc != 1) return 1;
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

    const auto* const unmappedMemory = reinterpret_cast<const FontMemory*>(0x10);
    FontMemory unmappedInterface = memory;
    unmappedInterface.iface = reinterpret_cast<const FontMemoryInterface*>(0x10);
    const void* const unmappedParams = reinterpret_cast<const void*>(0x10);
    FontLibrary rejectedLibrary = nullptr;
    errno = 71;
    Require(sceFontCreateLibraryWithEdition(unmappedMemory, sceFontSelectLibraryFt(0), 0, &rejectedLibrary) == SCE_FONT_ERROR_INVALID_MEMORY);
    Require(errno == 71 && rejectedLibrary == nullptr);
    errno = 71;
    Require(sceFontCreateLibraryWithEdition(&unmappedInterface, sceFontSelectLibraryFt(0), 0, &rejectedLibrary) == SCE_FONT_ERROR_INVALID_MEMORY);
    Require(errno == 71 && rejectedLibrary == nullptr);
    errno = 71;
    Require(sceFontCreateLibraryWithEdition(&memory, unmappedParams, 0, &rejectedLibrary) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(errno == 71 && rejectedLibrary == nullptr);
    errno = 71;
    Require(sceFontCreateLibraryWithEdition(&memory, DriverWithUnreadableTerm(sceFontSelectLibraryFt(0)), 0, &rejectedLibrary) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(errno == 71 && rejectedLibrary == nullptr);
    FontRenderer rejectedRenderer = nullptr;
    errno = 71;
    Require(sceFontCreateRendererWithEdition(unmappedMemory, sceFontSelectRendererFt(0), 0, &rejectedRenderer) == SCE_FONT_ERROR_INVALID_MEMORY);
    Require(errno == 71 && rejectedRenderer == nullptr);
    errno = 71;
    Require(sceFontCreateRendererWithEdition(&unmappedInterface, sceFontSelectRendererFt(0), 0, &rejectedRenderer) == SCE_FONT_ERROR_INVALID_MEMORY);
    Require(errno == 71 && rejectedRenderer == nullptr);
    errno = 71;
    Require(sceFontCreateRendererWithEdition(&memory, unmappedParams, 0, &rejectedRenderer) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(errno == 71 && rejectedRenderer == nullptr);

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

    std::uint32_t zeroProbe = 7;
    Require(sceFontRendererGetOutlineBufferSize(nullptr, &zeroProbe) == SCE_FONT_ERROR_INVALID_RENDERER && zeroProbe == 0);
    Require(sceFontRendererGetOutlineBufferSize(nullptr, nullptr) == SCE_FONT_ERROR_INVALID_RENDERER);
    Require(sceFontRendererGetOutlineBufferSize(renderer, nullptr) == sceFontRendererGetOutlineBufferSize(renderer, unmappedCount));
    zeroProbe = 7;
    Require(sceFontGetPixelResolution(nullptr, &zeroProbe) == SCE_FONT_ERROR_INVALID_LIBRARY && zeroProbe == 0);
    Require(sceFontGetPixelResolution(library, nullptr) == sceFontGetPixelResolution(library, unmappedCount));
    Require(sceFontGetPixelResolution(nullptr, nullptr) == sceFontGetPixelResolution(nullptr, unmappedCount));
    FontLibrary libraryProbe = reinterpret_cast<FontLibrary>(0x1);
    Require(sceFontGetLibrary(nullptr, &libraryProbe) == SCE_FONT_ERROR_INVALID_FONT_HANDLE && libraryProbe == nullptr);
    Require(sceFontGetLibrary(nullptr, nullptr) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    zeroProbe = 7;
    Require(sceFontGetFontGlyphsCount(nullptr, &zeroProbe) == SCE_FONT_ERROR_INVALID_FONT_HANDLE && zeroProbe == 0);
    Require(sceFontGetFontGlyphsCount(nullptr, nullptr) == sceFontGetFontGlyphsCount(nullptr, unmappedCount));

    std::uint32_t size = 7;
    Require(sceFontRendererGetOutlineBufferSize(renderer, &size) == SCE_FONT_OK && size == 0x4000);
    std::uint32_t subPixelCount = 1;
    Require(sceFontGetPixelResolution(library, &subPixelCount) == SCE_FONT_OK && subPixelCount == 64);

    auto* const unmappedAttribute = reinterpret_cast<std::int32_t*>(0x10);
    Require(sceFontSupportExternalFonts(library, 1, 0x52) == SCE_FONT_OK);
    const std::vector<unsigned char> fontData = BuildCharacterFont();
    FontHandle font = nullptr;
    Require(sceFontOpenFontMemory(library, fontData.data(), static_cast<std::uint32_t>(fontData.size()), nullptr, &font) == SCE_FONT_OK);
    Require(sceFontSetScalePixel(font, 100.0f, 100.0f) == SCE_FONT_OK);
    auto* const unmappedHandle = reinterpret_cast<FontHandle*>(0x10);
    const auto fontSize = static_cast<std::uint32_t>(fontData.size());
    FontHandle detailed = reinterpret_cast<FontHandle>(0x1);
    errno = 71;
    Require(sceFontOpenFontMemory(library, fontData.data(), fontSize, nullptr, unmappedHandle) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontOpenFontMemory(library, fontData.data(), fontSize, reinterpret_cast<const FontOpenDetail*>(0x10), &detailed) == SCE_FONT_ERROR_INVALID_PARAMETER && detailed == nullptr);
    Require(sceFontOpenFontFile(library, "/app0/missing.ttf", 1, nullptr, unmappedHandle) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontOpenFontInstance(font, nullptr, unmappedHandle) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontOpenFontSet(library, 0, 1, nullptr, unmappedHandle) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(errno == 71);
#ifdef __linux__
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    void* const slotPage = mmap(nullptr, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(slotPage != MAP_FAILED);
    auto* const lateSlot = static_cast<FontHandle*>(slotPage);
    const int allocationsBefore = liveAllocations;
    lockOnAllocate = slotPage;
    Require(sceFontOpenFontMemory(library, fontData.data(), fontSize, nullptr, lateSlot) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(lockOnAllocate == nullptr && *lateSlot == nullptr && liveAllocations == allocationsBefore);
    Require(munmap(slotPage, page) == 0);
#endif
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

    FontGlyph other = reinterpret_cast<FontGlyph>(0x1);
    FontGenerateGlyphDetail detail{};
    detail.id = 0x0FD3;
    detail.glyph_form = 1;
    detail.metrics_form = 1;
    detail.mem = reinterpret_cast<const FontMemory*>(0x10);
    errno = 71;
    Require(sceFontGenerateCharGlyph(font, 'A', nullptr, reinterpret_cast<FontGlyph*>(0x10)) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontGenerateCharGlyph(font, 'A', reinterpret_cast<const FontGenerateGlyphDetail*>(0x10), &other) == SCE_FONT_ERROR_INVALID_PARAMETER && other == nullptr);
    Require(sceFontGenerateCharGlyph(font, 'A', &detail, &other) == SCE_FONT_ERROR_INVALID_PARAMETER && other == nullptr);
    Require(sceFontDeleteGlyph(&memory, reinterpret_cast<FontGlyph*>(0x10)) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(errno == 71);
    Require(sceFontDeleteGlyph(&memory, &glyph) == SCE_FONT_OK && glyph == nullptr);
    Require(sceFontCloseFont(font) == SCE_FONT_OK);

    Require(sceFontDestroyRenderer(&renderer) == SCE_FONT_OK && renderer == nullptr);
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK && library == nullptr);

    auto* const pagedDriver = DriverOnOwnPage(sceFontSelectLibraryFt(0));
    FontLibrary pagedLibrary = nullptr;
    Require(sceFontCreateLibrary(&memory, pagedDriver, &pagedLibrary) == SCE_FONT_OK && pagedLibrary != nullptr);
    SetDriverReadable(pagedDriver, false);
    errno = 71;
    Require(sceFontDestroyLibrary(&pagedLibrary) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(errno == 71 && pagedLibrary != nullptr);
    SetDriverReadable(pagedDriver, true);
    Require(sceFontDestroyLibrary(&pagedLibrary) == SCE_FONT_OK && pagedLibrary == nullptr);
    ReleaseDriverPage(pagedDriver);
    return 0;
}
