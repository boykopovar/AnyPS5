#include "prx/libfreetype/libfreetype.hpp"
#include "prx/libSceFreeType/libSceFreeType.hpp"
#include "FreeTypeFontFixture.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_LCD_FILTER_H
#include FT_MULTIPLE_MASTERS_H

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <source_location>
#include <vector>

#ifdef _WIN32
#include "prx/libkernel/libkernel.hpp"
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Legacy FreeType library check failed at line %u\n", location.line());
        std::abort();
    }
}

static auto Load(const char* path) {
#ifdef _WIN32
    auto module = LoadLibraryW(std::filesystem::path(path).c_str());
    if (!module) std::fprintf(stderr, "LoadLibraryW failed: %lu\n", GetLastError());
#else
    auto module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
#endif
    Require(module != nullptr);
    return module;
}

template <typename TFunction, typename TModule>
static TFunction Import(TModule module, const char* name) {
#ifdef _WIN32
    const auto address = GetProcAddress(module, name);
#else
    const auto address = dlsym(module, name);
#endif
    if (!address) std::fprintf(stderr, "Missing symbol: %s\n", name);
    Require(address != nullptr);
    return std::bit_cast<TFunction>(address);
}

struct Api {
    decltype(&FT_Init_FreeType_nid_postfix) initialize;
    decltype(&FT_Done_FreeType_nid_postfix) doneLibrary;
    decltype(&FT_New_Memory_Face_nid_postfix) newFace;
    decltype(&FT_Done_Face_nid_postfix) doneFace;
    decltype(&FT_Set_Pixel_Sizes_nid_postfix) pixelSizes;
    decltype(&FT_Load_Char_nid_postfix) loadChar;
    decltype(&FT_Render_Glyph_nid_postfix) render;
    decltype(&freetypeLegacyLibrarySetLcdGeometry_nid_no_patch) geometry;
    decltype(&libfreetypeGetVarAxisFlags_nid_no_patch) axisFlags;
};

using Geometry = std::array<FreeType::GuestVector, 3>;

struct Rendered {
    std::uint32_t rows;
    std::uint32_t width;
    std::int32_t pitch;
    unsigned char pixelMode;
    std::int32_t left;
    std::int32_t top;
    std::vector<unsigned char> pixels;

    bool operator==(const Rendered&) const = default;
};

static Rendered Capture(std::uint32_t rows, std::uint32_t width, std::int32_t pitch, unsigned char pixelMode, std::int32_t left, std::int32_t top,
                        const unsigned char* buffer) {
    Rendered result{rows, width, pitch, pixelMode, left, top, {}};
    const std::size_t stride = static_cast<std::size_t>(pitch < 0 ? -pitch : pitch);
    if (buffer) result.pixels.assign(buffer, buffer + stride * rows);
    return result;
}

static void CheckGeometryErrors(const Api& api, FreeType::GuestLibrary* library) {
    Geometry geometry{{{-21, 0}, {0, 0}, {21, 0}}};
    Require(api.geometry(nullptr, nullptr) == FT_Library_SetLcdGeometry(nullptr, nullptr));
    Require(api.geometry(nullptr, nullptr) == FT_Err_Invalid_Library_Handle);
    Require(api.geometry(nullptr, geometry.data()) == FT_Err_Invalid_Library_Handle);
    Require(api.geometry(library, nullptr) == FT_Err_Invalid_Argument);
}

static Rendered RenderGuest(const Api& api, FreeType::GuestFaceRec* face, std::int32_t mode) {
    Require(api.loadChar(face, 'A', FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) == 0);
    Require(api.render(face->glyph, mode) == 0);
    const auto& bitmap = face->glyph->bitmap;
    return Capture(bitmap.rows, bitmap.width, bitmap.pitch, bitmap.pixel_mode, face->glyph->bitmap_left, face->glyph->bitmap_top, bitmap.buffer);
}

static Rendered RenderNative(FT_Face face, FT_Render_Mode mode) {
    Require(FT_Load_Char(face, 'A', FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP) == 0);
    Require(FT_Render_Glyph(face->glyph, mode) == 0);
    const auto& bitmap = face->glyph->bitmap;
    return Capture(bitmap.rows, bitmap.width, bitmap.pitch, bitmap.pixel_mode, face->glyph->bitmap_left, face->glyph->bitmap_top, bitmap.buffer);
}

static void CheckGeometry(const Api& api) {
    const auto font = SquareGlyphFont('A', 700);
    FreeType::GuestLibrary* library = nullptr;
    FreeType::GuestLibrary* other = nullptr;
    FreeType::GuestFaceRec* face = nullptr;
    FreeType::GuestFaceRec* otherFace = nullptr;
    FT_Library nativeLibrary = nullptr;
    FT_Face native = nullptr;
    Require(api.initialize(&library) == 0 && api.initialize(&other) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    CheckGeometryErrors(api, library);
    Require(api.newFace(library, font.data(), static_cast<std::int64_t>(font.size()), 0, &face) == 0);
    Require(api.newFace(other, font.data(), static_cast<std::int64_t>(font.size()), 0, &otherFace) == 0);
    Require(FT_New_Memory_Face(nativeLibrary, font.data(), static_cast<FT_Long>(font.size()), 0, &native) == 0);
    const std::array<Geometry, 6> geometries{{
        {{{-21, 0}, {0, 0}, {21, 0}}},
        {{{-64, 0}, {0, 0}, {64, 0}}},
        {{{0, -40}, {0, 0}, {0, 40}}},
        {{{30, 10}, {-5, 0}, {-30, -10}}},
        {{{0, 0}, {0, 0}, {0, 0}}},
        {{{-200, 96}, {13, -7}, {150, -96}}},
    }};
    for (const std::uint32_t pixels : {13u, 32u}) {
        Require(api.pixelSizes(face, 0, pixels) == 0 && api.pixelSizes(otherFace, 0, pixels) == 0 && FT_Set_Pixel_Sizes(native, 0, pixels) == 0);
        for (const auto mode : {FT_RENDER_MODE_LCD, FT_RENDER_MODE_LCD_V}) {
            std::vector<Rendered> outputs;
            for (const auto& geometry : geometries) {
                const Geometry before = geometry;
                Require(api.geometry(library, geometry.data()) == 0);
                Require(std::memcmp(before.data(), geometry.data(), sizeof(Geometry)) == 0);
                std::array<FT_Vector, 3> vectors{};
                for (std::size_t index = 0; index < vectors.size(); ++index) {
                    vectors[index] = {static_cast<FT_Pos>(geometry[index].x), static_cast<FT_Pos>(geometry[index].y)};
                }
                Require(FT_Library_SetLcdGeometry(nativeLibrary, vectors.data()) == 0);
                const Rendered actual = RenderGuest(api, face, mode);
                Require(actual == RenderNative(native, mode));
                Require(actual.width > 0 && actual.rows > 0 && !actual.pixels.empty());
                outputs.push_back(actual);
            }
            Require(outputs[1] != outputs[0] && outputs[2] != outputs[0] && outputs[4] != outputs[0] && outputs[5] != outputs[0]);
            Require(RenderGuest(api, otherFace, mode) == outputs[0]);
            Require(api.geometry(other, geometries[1].data()) == 0);
            Require(RenderGuest(api, otherFace, mode) == outputs[1]);
            Require(RenderGuest(api, face, mode) == outputs[5]);
            Require(api.geometry(other, geometries[0].data()) == 0);
            Require(api.geometry(library, geometries[0].data()) == 0);
            Require(RenderGuest(api, face, mode) == outputs[0]);
        }
    }
    Require(api.doneFace(face) == 0 && api.doneFace(otherFace) == 0 && FT_Done_Face(native) == 0);
    Require(api.doneLibrary(library) == 0 && api.doneLibrary(other) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
}

struct alignas(16) MasterBuffer {
    FT_MM_Var master;
    std::array<FT_UShort, 8> flags;
};

static_assert(offsetof(MasterBuffer, flags) == sizeof(FT_MM_Var));

static MasterBuffer Master(FT_UInt axes) {
    MasterBuffer buffer{};
    buffer.master.num_axis = axes;
    buffer.master.num_designs = 3;
    buffer.master.num_namedstyles = 2;
    buffer.master.axis = reinterpret_cast<FT_Var_Axis*>(0x1000);
    buffer.master.namedstyle = reinterpret_cast<FT_Var_Named_Style*>(0x2000);
    buffer.flags = {0x0000, FT_VAR_AXIS_FLAG_HIDDEN, 0x8001, 0xFFFF, 0x0002, 0x7FFE, 0x1234, 0xFFFE};
    return buffer;
}

static void CheckAxisFlags(const Api& api) {
    constexpr std::uint32_t sentinel = 0xA5A5A5A5u;
    for (const FT_UInt axes : {0u, 1u, 4u, 8u}) {
        MasterBuffer buffer = Master(axes);
        const MasterBuffer before = buffer;
        for (const std::uint32_t axis : {0u, 1u, 2u, 3u, 4u, 7u, 8u, 9u, 0x7FFFFFFFu, std::numeric_limits<std::uint32_t>::max()}) {
            std::uint32_t actual = sentinel;
            FT_UInt expected = sentinel;
            const int error = api.axisFlags(&buffer.master, axis, &actual);
            Require(error == FT_Get_Var_Axis_Flags(&buffer.master, axis, &expected));
            Require(actual == expected);
            Require(error == (axis < axes ? 0 : FT_Err_Invalid_Argument));
            if (axis < axes) Require(actual == buffer.flags[axis]);
            Require(std::memcmp(&buffer, &before, sizeof(buffer)) == 0);
        }
        std::uint32_t untouched = sentinel;
        Require(api.axisFlags(nullptr, 0, &untouched) == FT_Err_Invalid_Argument && untouched == sentinel);
        Require(api.axisFlags(&buffer.master, 0, nullptr) == FT_Get_Var_Axis_Flags(&buffer.master, 0, nullptr));
        Require(api.axisFlags(&buffer.master, 0, nullptr) == FT_Err_Invalid_Argument);
        Require(api.axisFlags(nullptr, 0, nullptr) == FT_Err_Invalid_Argument);
    }
}

#ifdef _WIN32
struct KernelApi {
    decltype(&sceKernelAllocateDirectMemory) allocate;
    decltype(&sceKernelMapDirectMemory) map;
    decltype(&sceKernelMprotect) protect;
    decltype(&sceKernelMunmap) unmap;
    decltype(&sceKernelReleaseDirectMemory) release;
};

class GuestPages {
public:
    static constexpr std::size_t Page = 0x4000;

    explicit GuestPages(const KernelApi& api) : api(api) {
        Require(api.allocate(0, 0x7fffffffffll, Page * 2, 0, 0, &physical) == 0);
        Require(api.map(&mapping, Page * 2, 3, 0, physical, 0) == 0);
        std::memset(mapping, 0xA5, Page * 2);
    }

    ~GuestPages() {
        Require(api.unmap(reinterpret_cast<std::uint64_t>(mapping), Page * 2) == 0);
        Require(api.release(physical, Page * 2) == 0);
    }

    template <typename TValue>
    TValue* At(unsigned page, std::size_t offset = 16) const {
        return reinterpret_cast<TValue*>(static_cast<unsigned char*>(mapping) + Page * page + offset);
    }

    void Protect(unsigned page, int protection) const {
        Require(api.protect(static_cast<unsigned char*>(mapping) + Page * page, Page, protection) == 0);
        if (protection == 1) {
            MEMORY_BASIC_INFORMATION information{};
            Require(VirtualQuery(At<unsigned char>(page), &information, sizeof(information)) == sizeof(information));
            Require(information.Protect == PAGE_READONLY);
        }
    }

    std::vector<unsigned char> Snapshot() const {
        const auto* bytes = static_cast<const unsigned char*>(mapping);
        return {bytes, bytes + Page * 2};
    }

private:
    const KernelApi& api;
    std::int64_t physical = 0;
    void* mapping = nullptr;
};

static void CheckGuestMemory(const Api& api, const KernelApi& kernel) {
    GuestPages pages(kernel);
    auto* master = pages.At<MasterBuffer>(0);
    *master = Master(4);
    auto* flags = pages.At<std::uint32_t>(1);
    *flags = 0x5A5A5A5Au;
    pages.Protect(0, 1);
    pages.Protect(1, 1);
    for (const std::uint32_t axis : {0u, 1u, 3u}) {
        const auto before = pages.Snapshot();
        Require(api.axisFlags(&master->master, axis, flags) == FT_Err_Invalid_Argument);
        Require(pages.Snapshot() == before);
    }
    const auto before = pages.Snapshot();
    Require(api.axisFlags(&master->master, 4, flags) == FT_Err_Invalid_Argument && pages.Snapshot() == before);
    std::uint32_t value = 0;
    Require(api.axisFlags(&master->master, 3, &value) == 0 && value == 0xFFFFu);
    pages.Protect(1, 3);
    Require(api.axisFlags(&master->master, 2, flags) == 0 && *flags == 0x8001u);
    Require(api.axisFlags(&master->master, 1, flags) == 0 && *flags == FT_VAR_AXIS_FLAG_HIDDEN);

    const auto font = SquareGlyphFont('A', 700);
    FreeType::GuestLibrary* library = nullptr;
    FreeType::GuestFaceRec* face = nullptr;
    FT_Library nativeLibrary = nullptr;
    FT_Face native = nullptr;
    Require(api.initialize(&library) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    Require(api.newFace(library, font.data(), static_cast<std::int64_t>(font.size()), 0, &face) == 0);
    Require(FT_New_Memory_Face(nativeLibrary, font.data(), static_cast<FT_Long>(font.size()), 0, &native) == 0);
    Require(api.pixelSizes(face, 0, 24) == 0 && FT_Set_Pixel_Sizes(native, 0, 24) == 0);
    auto* geometry = pages.At<FreeType::GuestVector>(0, 256);
    pages.Protect(0, 3);
    geometry[0] = {-48, 0};
    geometry[1] = {0, 0};
    geometry[2] = {48, 0};
    pages.Protect(0, 1);
    const auto readOnly = pages.Snapshot();
    Require(api.geometry(library, geometry) == 0);
    Require(pages.Snapshot() == readOnly);
    std::array<FT_Vector, 3> vectors{{{-48, 0}, {0, 0}, {48, 0}}};
    Require(FT_Library_SetLcdGeometry(nativeLibrary, vectors.data()) == 0);
    Require(RenderGuest(api, face, FT_RENDER_MODE_LCD) == RenderNative(native, FT_RENDER_MODE_LCD));
    pages.Protect(0, 3);
    Require(api.doneFace(face) == 0 && FT_Done_Face(native) == 0);
    Require(api.doneLibrary(library) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
}
#endif

int main(int argc, char** argv) {
    Require(argc == 4);
    const bool patched = std::strcmp(argv[3], "patched") == 0;
    Require(patched || std::strcmp(argv[3], "unpatched") == 0);
    auto module = Load(argv[1]);
    auto base = Load(argv[2]);
#ifdef _WIN32
    auto kernel = Load((std::filesystem::path(argv[2]).parent_path() / "libkernel.prx").string().c_str());
    const KernelApi kernelApi{
        Import<decltype(KernelApi::allocate)>(kernel, patched ? "rTXw65xmLIA" : "sceKernelAllocateDirectMemory"),
        Import<decltype(KernelApi::map)>(kernel, patched ? "L-Q3LEjIbgA" : "sceKernelMapDirectMemory"),
        Import<decltype(KernelApi::protect)>(kernel, patched ? "vSMAm3cxYTY" : "sceKernelMprotect"),
        Import<decltype(KernelApi::unmap)>(kernel, patched ? "cQke9UuBQOk" : "sceKernelMunmap"),
        Import<decltype(KernelApi::release)>(kernel, patched ? "MBuItvba6z8" : "sceKernelReleaseDirectMemory")
    };
#endif
    Api api{};
    api.initialize = Import<decltype(api.initialize)>(base, patched ? "GNsiLzm4SH8" : "FT_Init_FreeType_nid_postfix");
    api.doneLibrary = Import<decltype(api.doneLibrary)>(base, patched ? "dH02CpEEcYk" : "FT_Done_FreeType_nid_postfix");
    api.newFace = Import<decltype(api.newFace)>(base, patched ? "40Wa2neSLM0" : "FT_New_Memory_Face_nid_postfix");
    api.doneFace = Import<decltype(api.doneFace)>(base, patched ? "OgFczHko0w8" : "FT_Done_Face_nid_postfix");
    api.pixelSizes = Import<decltype(api.pixelSizes)>(base, patched ? "lJxMIwj81SQ" : "FT_Set_Pixel_Sizes_nid_postfix");
    api.loadChar = Import<decltype(api.loadChar)>(base, patched ? "wPQ0rMK4-JU" : "FT_Load_Char_nid_postfix");
    api.render = Import<decltype(api.render)>(base, patched ? "TCRmigbq7lc" : "FT_Render_Glyph_nid_postfix");
    api.geometry = Import<decltype(api.geometry)>(module, patched ? "Xxtq+Qr5lzI" : "Xxtq+Qr5lzI_nid_no_patch_cut");
    api.axisFlags = Import<decltype(api.axisFlags)>(module, patched ? "+sDVkl3aAR8" : "+sDVkl3aAR8_nid_no_patch_cut");
    CheckGeometry(api);
    CheckAxisFlags(api);
#ifdef _WIN32
    CheckGuestMemory(api, kernelApi);
    Require(FreeLibrary(base) != 0 && FreeLibrary(module) != 0 && FreeLibrary(kernel) != 0);
#else
    Require(dlclose(base) == 0 && dlclose(module) == 0);
#endif
}
