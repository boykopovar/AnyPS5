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
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font close unlink check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

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
    detail.subfont_index = 0;
    FontHandle sharedFirst = nullptr;
    FontHandle sharedSecond = nullptr;
    Require(sceFontOpenFontMemory(library, collection.data(), static_cast<std::uint32_t>(collection.size()), &detail, &sharedFirst) == SCE_FONT_OK);
    Require(sceFontOpenFontMemory(library, collection.data(), static_cast<std::uint32_t>(collection.size()), &detail, &sharedSecond) == SCE_FONT_OK);
    Font::FontObj* shared = FontHead(sharedSecond);
    Require(shared && shared == FontHead(sharedFirst) && shared->refcount == 2 && closePages.count(shared) == 1);
    Require(SetWritable(shared, false));
    errno = 71;
    Require(sceFontCloseFont(sharedSecond) == SCE_FONT_ERROR_FATAL && errno == 71);
    Require(shared->refcount == 2 && FontHead(sharedSecond) == shared);
    Require(reinterpret_cast<const Font::FontHandleNative*>(sharedSecond)->magic == Font::HANDLE_MAGIC);
    Require(SetWritable(shared, true));
    Require(sceFontCloseFont(sharedSecond) == SCE_FONT_OK && shared->refcount == 1);
    Require(sceFontCloseFont(sharedFirst) == SCE_FONT_OK && closePages.empty());
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK);
}

int main() {
    CheckCloseUnlink();
    return 0;
}
