#include "prx/libfreetype/libfreetype.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libkernel/libkernel.hpp"

#include FT_GLYPH_H
#include FT_MODULE_H

#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using namespace FreeType;

namespace {

void Require(bool condition, const char* message, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "FreeType new glyph check failed at line %u: %s\n", location.line(), message);
    std::abort();
}

class Module {
public:
    explicit Module(const char* path) {
#ifdef _WIN32
        handle = LoadLibraryW(std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(path))).c_str());
        if (!handle) std::fprintf(stderr, "LoadLibraryW failed for %s: %lu\n", path, GetLastError());
#else
        handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!handle) std::fprintf(stderr, "dlopen failed for %s: %s\n", path, dlerror());
#endif
        Require(handle != nullptr, "load legacy module");
    }

    ~Module() {
#ifdef _WIN32
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
    }

    auto Import(const char* name) const {
#ifdef _WIN32
        auto symbol = GetProcAddress(handle, name);
#else
        auto symbol = dlsym(handle, name);
#endif
        Require(symbol != nullptr, name);
        return std::bit_cast<decltype(&freetypeLegacyNewGlyph_nid_no_patch)>(symbol);
    }

private:
#ifdef _WIN32
    HMODULE handle = nullptr;
#else
    void* handle = nullptr;
#endif
};

using NewGlyph = decltype(&freetypeLegacyNewGlyph_nid_no_patch);

struct Output {
    std::uint64_t before = 0xA5A5B4B4C3C3D2D2;
    GuestGlyphRec* glyph = reinterpret_cast<GuestGlyphRec*>(std::uintptr_t{0x12345678});
    std::uint64_t after = 0x1E1E2D2D3C3C4B4B;

    void Guards() const {
        Require(before == 0xA5A5B4B4C3C3D2D2 && after == 0x1E1E2D2D3C3C4B4B, "surrounding output guards unchanged");
    }
};

void Matches(const GuestGlyphRec* glyph, FT_Glyph native, GuestLibrary* library) {
    Require(glyph && native && glyph->library == library && glyph->clazz, "owned library and non-null class");
    Require(glyph->format == static_cast<std::uint32_t>(native->format), "glyph format");
    Require(glyph->advance.x == native->advance.x && glyph->advance.y == native->advance.y, "glyph advance");
    if (native->format == FT_GLYPH_FORMAT_BITMAP) {
        const auto& image = *reinterpret_cast<const GuestBitmapGlyphRec*>(glyph);
        const auto host = reinterpret_cast<FT_BitmapGlyph>(native);
        Require(image.left == host->left && image.top == host->top, "bitmap placement");
        const auto& a = image.bitmap;
        const auto& b = host->bitmap;
        Require(a.rows == b.rows && a.width == b.width && a.pitch == b.pitch, "bitmap dimensions");
        Require(a.num_grays == b.num_grays && a.pixel_mode == b.pixel_mode && a.palette_mode == b.palette_mode && a.palette == b.palette, "bitmap metadata");
        Require((a.buffer == nullptr) == (b.buffer == nullptr), "bitmap storage presence");
        if (a.buffer && a.rows) Require(std::memcmp(a.buffer, b.buffer, a.rows * static_cast<std::size_t>(a.pitch)) == 0, "bitmap payload");
    } else {
        const auto& a = reinterpret_cast<const GuestOutlineGlyphRec*>(glyph)->outline;
        const auto& b = reinterpret_cast<FT_OutlineGlyph>(native)->outline;
        Require(a.n_contours == b.n_contours && a.n_points == b.n_points && a.flags == b.flags, "outline dimensions and flags");
        Require((a.points == nullptr) == (b.points == nullptr) && (a.tags == nullptr) == (b.tags == nullptr) &&
                (a.contours == nullptr) == (b.contours == nullptr), "outline storage presence");
        for (std::size_t index = 0; index < a.n_points; ++index) {
            Require(a.points[index].x == b.points[index].x && a.points[index].y == b.points[index].y, "outline points");
            Require(a.tags[index] == static_cast<unsigned char>(b.tags[index]), "outline tags");
        }
        for (std::size_t index = 0; index < a.n_contours; ++index) Require(a.contours[index] == b.contours[index], "outline contours");
    }
}

void Arguments(NewGlyph call, GuestLibrary* library, FT_Library host) {
    for (const auto format : {FT_GLYPH_FORMAT_BITMAP, FT_GLYPH_FORMAT_OUTLINE, FT_GLYPH_FORMAT_NONE,
                              static_cast<FT_Glyph_Format>(0x12345678)}) {
        Output output;
        const auto original = output.glyph;
        FT_Glyph native = reinterpret_cast<FT_Glyph>(original);
        Require(call(nullptr, format, &output.glyph) == FT_New_Glyph(nullptr, format, &native), "null library error");
        Require(output.glyph == original && native == reinterpret_cast<FT_Glyph>(original), "null library preserves output");
        Require(call(library, format, nullptr) == FT_New_Glyph(host, format, nullptr), "null output error");
        output.Guards();
    }
    for (const auto format : {FT_GLYPH_FORMAT_NONE, FT_GLYPH_FORMAT_COMPOSITE, FT_GLYPH_FORMAT_PLOTTER,
                              static_cast<FT_Glyph_Format>(0x12345678), static_cast<FT_Glyph_Format>(0xFFFFFFFFu)}) {
        Output output;
        const auto original = output.glyph;
        FT_Glyph native = reinterpret_cast<FT_Glyph>(original);
        Require(call(library, format, &output.glyph) == FT_New_Glyph(host, format, &native), "unsupported format error");
        Require(output.glyph == original && native == reinterpret_cast<FT_Glyph>(original), "unsupported format preserves output");
        output.Guards();
    }
    GuestLibrary empty;
    Output output;
    const auto original = output.glyph;
    Require(call(&empty, FT_GLYPH_FORMAT_BITMAP, &output.glyph) == FT_Err_Invalid_Argument, "missing native library");
    Require(output.glyph == original, "missing native library preserves output");
}

void SvgClass(NewGlyph call, GuestLibrary* library, FT_Library host) {
    Output output;
    const auto original = output.glyph;
    FT_Glyph native = nullptr;
    const auto expected = FT_New_Glyph(host, FT_GLYPH_FORMAT_SVG, &native);
    if (expected == FT_Err_Invalid_Glyph_Format) {
        Require(call(library, FT_GLYPH_FORMAT_SVG, &output.glyph) == expected, "unavailable native SVG class error");
    } else {
        Require(expected == 0 && native != nullptr, "native SVG class construction");
        FT_Done_Glyph(native);
        try {
            call(library, FT_GLYPH_FORMAT_SVG, &output.glyph);
        } catch (const std::exception&) {}
    }
    Require(output.glyph == original, "unsupported SVG preserves output");
    output.Guards();
}

void CreationAndCopy(NewGlyph call, GuestLibrary* library, FT_Library host, FT_Glyph_Format format) {
    Output first;
    Output second;
    FT_Glyph native = nullptr;
    Require(call(library, format, &first.glyph) == FT_New_Glyph(host, format, &native), "create zeroed glyph");
    Require(call(library, format, &second.glyph) == 0, "create independent glyph");
    Require(first.glyph != second.glyph, "independent record addresses");
    Matches(first.glyph, native, library);
    Matches(second.glyph, native, library);
    first.Guards();
    second.Guards();

    std::array<unsigned char, 6> pixels{1, 7, 15, 31, 63, 127};
    std::array<GuestVector, 3> points{{{-31, 27}, {128, -64}, {9, 212}}};
    std::array<FT_Vector, 3> nativePoints{{{-31, 27}, {128, -64}, {9, 212}}};
    std::array<unsigned char, 3> tags{1, 0, 1};
    std::array<std::uint16_t, 1> contours{2};
    std::array<unsigned short, 1> nativeContours{2};
    first.glyph->advance = {-123456, 98765};
    native->advance = {-123456, 98765};
    if (format == FT_GLYPH_FORMAT_BITMAP) {
        auto& image = *reinterpret_cast<GuestBitmapGlyphRec*>(first.glyph);
        auto hostImage = reinterpret_cast<FT_BitmapGlyph>(native);
        image.left = hostImage->left = -7;
        image.top = hostImage->top = 11;
        image.bitmap = {2, 3, 3, pixels.data(), 256, FT_PIXEL_MODE_GRAY, 0, nullptr};
        hostImage->bitmap = {2, 3, 3, pixels.data(), 256, FT_PIXEL_MODE_GRAY, 0, nullptr};
    } else {
        reinterpret_cast<GuestOutlineGlyphRec*>(first.glyph)->outline = {1, 3, points.data(), tags.data(), contours.data(), 0};
        reinterpret_cast<FT_OutlineGlyph>(native)->outline = {1, 3, nativePoints.data(), reinterpret_cast<decltype(FT_Outline{}.tags)>(tags.data()), nativeContours.data(), 0};
    }
    GuestGlyphRec* copy = nullptr;
    FT_Glyph nativeCopy = nullptr;
    Require(FT_Glyph_Copy_nid_postfix(first.glyph, &copy) == FT_Glyph_Copy(native, &nativeCopy), "copy newly created glyph");
    Require(copy != first.glyph && copy != second.glyph, "copy is independent");
    Matches(copy, nativeCopy, library);
    if (format == FT_GLYPH_FORMAT_BITMAP) {
        Require(reinterpret_cast<GuestBitmapGlyphRec*>(copy)->bitmap.buffer != pixels.data(), "bitmap copy owns storage");
        reinterpret_cast<GuestBitmapGlyphRec*>(first.glyph)->bitmap = {};
        reinterpret_cast<FT_BitmapGlyph>(native)->bitmap = {};
    } else {
        Require(reinterpret_cast<GuestOutlineGlyphRec*>(copy)->outline.points != points.data(), "outline copy owns points");
        reinterpret_cast<GuestOutlineGlyphRec*>(first.glyph)->outline = {};
        reinterpret_cast<FT_OutlineGlyph>(native)->outline = {};
    }
    pixels.fill(0xCC);
    points.fill({777, -888});
    tags.fill(0);
    FT_Done_Glyph_nid_postfix(first.glyph);
    FT_Done_Glyph(native);
    FT_Done_Glyph_nid_postfix(second.glyph);
    Matches(copy, nativeCopy, library);
    FT_Done_Glyph_nid_postfix(copy);
    FT_Done_Glyph(nativeCopy);
}

struct Allocator {
    bool fail = false;
    std::size_t attempts = 0;
    std::size_t live = 0;
    FT_MemoryRec_ memory{this, Allocate, Free, Reallocate};

    static void* Allocate(FT_Memory memory, long size) {
        auto& self = *static_cast<Allocator*>(memory->user);
        ++self.attempts;
        if (self.fail) return nullptr;
        auto* result = std::malloc(static_cast<std::size_t>(size));
        if (result) ++self.live;
        return result;
    }

    static void Free(FT_Memory memory, void* block) {
        if (!block) return;
        auto& self = *static_cast<Allocator*>(memory->user);
        Require(self.live != 0, "allocator release matches allocation");
        --self.live;
        std::free(block);
    }

    static void* Reallocate(FT_Memory memory, long, long size, void* block) {
        auto& self = *static_cast<Allocator*>(memory->user);
        ++self.attempts;
        if (self.fail) return nullptr;
        auto* result = std::realloc(block, static_cast<std::size_t>(size));
        if (result && !block) ++self.live;
        return result;
    }
};

void AllocationFailure(NewGlyph call) {
    Allocator allocator;
    GuestLibrary library;
    Require(FT_New_Library(&allocator.memory, &library.host) == 0, "create recording allocator library");
    for (const auto format : {FT_GLYPH_FORMAT_BITMAP, FT_GLYPH_FORMAT_OUTLINE}) {
        allocator.fail = true;
        const auto live = allocator.live;
        const auto attempts = allocator.attempts;
        Output output;
        FT_Glyph native = reinterpret_cast<FT_Glyph>(output.glyph);
        const auto expected = FT_New_Glyph(library.host, format, &native);
        Require(expected == FT_Err_Out_Of_Memory && native == nullptr, "native allocation failure clears output");
        Require(call(&library, format, &output.glyph) == expected, "allocation failure error");
        Require(output.glyph == nullptr && allocator.live == live && allocator.attempts >= attempts + 2, "failed allocation clears output and leaks no native glyph");
        output.Guards();
        allocator.fail = false;
        Require(call(&library, format, &output.glyph) == 0, "creation succeeds after allocation failure");
        FT_Done_Glyph_nid_postfix(output.glyph);
        Require(allocator.live == live, "successful glyph released through native allocator");
    }
    Require(FT_Done_Library(library.host) == 0, "destroy recording allocator library");
    Require(allocator.live == 0, "all native allocations released");
}

#ifdef _WIN32
void ReadOnlyOutput(NewGlyph call, GuestLibrary* library) {
    constexpr std::size_t bytes = 0x4000;
    std::int64_t physical = 0;
    void* mapping = nullptr;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, bytes, 0, 0, &physical) == 0, "allocate guest output page");
    Require(sceKernelMapDirectMemory(&mapping, bytes, 3, 0, physical, 0) == 0, "map guest output page");
    std::memset(mapping, 0xA5, bytes);
    auto** output = reinterpret_cast<GuestGlyphRec**>(static_cast<unsigned char*>(mapping) + 32);
    const auto* begin = static_cast<const unsigned char*>(mapping);
    const std::vector<unsigned char> original(begin, begin + bytes);
    Require(sceKernelMprotect(mapping, bytes, 1) == 0, "protect guest output page");
    for (const bool hostWritable : {true, false}) {
        if (hostWritable) {
            DWORD previous = 0;
            Require(VirtualProtect(mapping, bytes, PAGE_READWRITE, &previous) != 0, "allow assertion-based host-write control");
        } else {
            Require(sceKernelMprotect(mapping, bytes, 3) == 0 && sceKernelMprotect(mapping, bytes, 1) == 0, "restore actual read-only page");
            MEMORY_BASIC_INFORMATION information{};
            Require(VirtualQuery(mapping, &information, sizeof(information)) == sizeof(information) && information.Protect == PAGE_READONLY, "verify actual read-only mapping");
        }
        for (const auto format : {FT_GLYPH_FORMAT_BITMAP, FT_GLYPH_FORMAT_OUTLINE}) {
            Require(call(library, format, output) == FT_Err_Invalid_Argument, "refused guest output returns error");
            Require(std::memcmp(mapping, original.data(), bytes) == 0, "refused guest output preserves whole page");
        }
    }
    Require(sceKernelMprotect(mapping, bytes, 3) == 0, "restore guest output write permission");
    Require(call(library, FT_GLYPH_FORMAT_BITMAP, output) == 0 && *output, "writable guest output succeeds");
    FT_Done_Glyph_nid_postfix(*output);
    std::memcpy(output, original.data() + 32, sizeof(*output));
    Require(std::memcmp(mapping, original.data(), bytes) == 0, "writable call only changes output pointer");
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(mapping), bytes) == 0, "unmap guest output page");
    Require(sceKernelReleaseDirectMemory(physical, bytes) == 0, "release guest output memory");
}
#endif

}

int main(int argc, char** argv) {
    Require(argc == 3, "module and variant arguments");
    const std::string_view variant = argv[2];
    Require(variant == "unpatched" || variant == "nid", "known variant");
    Module module(argv[1]);
    const auto call = module.Import(variant == "nid" ? "sZq9oSwMGSs" : "sZq9oSwMGSs_nid_no_patch_cut");
    GuestLibrary* library = nullptr;
    FT_Library host = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&library) == 0 && FT_Init_FreeType(&host) == 0, "initialize libraries");
    try {
        Arguments(call, library, host);
        SvgClass(call, library, host);
        CreationAndCopy(call, library, host, FT_GLYPH_FORMAT_BITMAP);
        CreationAndCopy(call, library, host, FT_GLYPH_FORMAT_OUTLINE);
        AllocationFailure(call);
#ifdef _WIN32
        ReadOnlyOutput(call, library);
#endif
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Unexpected glyph exception: %s\n", error.what());
        Require(false, "new glyph export must implement supported formats without throwing");
    }
    Require(FT_Done_FreeType_nid_postfix(library) == 0 && FT_Done_FreeType(host) == 0, "destroy libraries");
    return 0;
}
