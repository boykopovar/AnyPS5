#include "prx/libfreetype/libfreetype.hpp"
#include "prx/libSceFreeType/libSceFreeType.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <source_location>
#include <vector>

#ifdef _WIN32
#include "prx/libkernel/libkernel.hpp"
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using FreeType::GuestBitmap;
using FreeType::GuestVector;
using LegacyFreeType::Color;

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Legacy FreeType bitmap blend check failed at line %u\n", location.line());
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
    decltype(&FT_Bitmap_Done_nid_postfix) doneBitmap;
    decltype(&freetypeLegacyBitmapBlend_nid_no_patch) blend;
};

struct Source {
    std::vector<unsigned char> pixels;
    GuestBitmap guest{};

    FT_Bitmap Native() const {
        return {guest.rows, guest.width, guest.pitch, guest.buffer, guest.num_grays, guest.pixel_mode, guest.palette_mode, guest.palette};
    }
};

static Source MakeSource(unsigned char mode, std::uint32_t rows, std::uint32_t width, bool negativePitch, std::uint32_t seed) {
    std::uint32_t stride = width;
    std::uint16_t grays = 256;
    switch (mode) {
        case FT_PIXEL_MODE_MONO: stride = (width + 7) / 8; grays = 2; break;
        case FT_PIXEL_MODE_GRAY2: stride = (width + 3) / 4; grays = 4; break;
        case FT_PIXEL_MODE_GRAY4: stride = (width + 1) / 2; grays = 16; break;
        case FT_PIXEL_MODE_BGRA: stride = width * 4; break;
        default: break;
    }
    stride += seed % 3;
    Source source;
    source.pixels.resize(static_cast<std::size_t>(stride) * rows);
    std::mt19937 random(seed);
    for (auto& pixel : source.pixels) pixel = static_cast<unsigned char>(random());
    if (seed % 5 == 0) {
        for (std::size_t index = 0; index < source.pixels.size(); index += 3) source.pixels[index] = index % 2 ? 0 : 255;
    }
    const int pitch = static_cast<int>(stride);
    source.guest = {rows, width, negativePitch ? -pitch : pitch, source.pixels.empty() ? nullptr : source.pixels.data(), grays, mode, 0, nullptr};
    return source;
}

static std::vector<unsigned char> Pixels(const unsigned char* buffer, std::uint32_t rows, std::int32_t pitch) {
    if (!buffer) return {};
    const std::size_t stride = static_cast<std::size_t>(pitch < 0 ? -static_cast<std::int64_t>(pitch) : pitch);
    return {buffer, buffer + stride * rows};
}

static void CheckSame(const GuestBitmap& guest, const FT_Bitmap& native, const GuestVector& guestOffset, const FT_Vector& nativeOffset) {
    Require(guest.rows == native.rows && guest.width == native.width && guest.pitch == native.pitch);
    Require(guest.num_grays == native.num_grays && guest.pixel_mode == native.pixel_mode && guest.palette_mode == native.palette_mode);
    Require((guest.buffer == nullptr) == (native.buffer == nullptr));
    Require(Pixels(guest.buffer, guest.rows, guest.pitch) == Pixels(native.buffer, native.rows, native.pitch));
    Require(guestOffset.x == nativeOffset.x && guestOffset.y == nativeOffset.y);
}

struct Step {
    unsigned char mode;
    std::uint32_t rows;
    std::uint32_t width;
    GuestVector offset;
    Color color;
};

static void CheckSequences(const Api& api, FreeType::GuestLibrary* library, FT_Library nativeLibrary) {
    const std::array<Color, 4> colors{{{255, 255, 255, 255}, {10, 200, 90, 255}, {255, 0, 128, 77}, {0, 0, 0, 0}}};
    const std::vector<std::vector<Step>> sequences{
        {{FT_PIXEL_MODE_GRAY, 9, 7, {0, 0}, colors[0]}},
        {{FT_PIXEL_MODE_GRAY, 5, 4, {64, 128}, colors[1]}, {FT_PIXEL_MODE_GRAY, 6, 3, {-128, 320}, colors[2]}, {FT_PIXEL_MODE_GRAY, 2, 9, {200, -70}, colors[0]}},
        {{FT_PIXEL_MODE_MONO, 7, 13, {32, 63}, colors[1]}, {FT_PIXEL_MODE_GRAY2, 4, 6, {-65, 0}, colors[2]}, {FT_PIXEL_MODE_GRAY4, 3, 5, {128, 128}, colors[3]}},
        {{FT_PIXEL_MODE_LCD, 4, 12, {0, 64}, colors[0]}, {FT_PIXEL_MODE_LCD_V, 9, 4, {256, -256}, colors[1]}, {FT_PIXEL_MODE_BGRA, 3, 3, {64, 64}, colors[2]}},
        {{FT_PIXEL_MODE_GRAY, 1, 1, {-640, 640}, colors[2]}, {FT_PIXEL_MODE_GRAY, 1, 1, {640, -640}, colors[1]}, {FT_PIXEL_MODE_GRAY, 4, 4, {0, 0}, colors[0]}},
        {{FT_PIXEL_MODE_GRAY, 8, 8, {0, 512}, colors[0]}, {FT_PIXEL_MODE_GRAY, 3, 3, {128, 384}, colors[1]}},
    };
    std::uint32_t seed = 1;
    for (const auto& sequence : sequences) {
        GuestBitmap guest{};
        FT_Bitmap native;
        FT_Bitmap_Init(&native);
        GuestVector guestOffset{0x1234, -0x5678};
        FT_Vector nativeOffset{0x1234, -0x5678};
        for (const auto& step : sequence) {
            const Source source = MakeSource(step.mode, step.rows, step.width, false, seed++);
            const auto before = source.pixels;
            const FT_Bitmap nativeSource = source.Native();
            const int error = api.blend(library, &source.guest, step.offset, &guest, &guestOffset, step.color);
            const FT_Color color{step.color.blue, step.color.green, step.color.red, step.color.alpha};
            Require(error == FT_Bitmap_Blend(nativeLibrary, &nativeSource, {static_cast<FT_Pos>(step.offset.x), static_cast<FT_Pos>(step.offset.y)}, &native,
                                             &nativeOffset, color));
            Require(error == 0 && guest.pixel_mode == FT_PIXEL_MODE_BGRA && guest.buffer != nullptr);
            CheckSame(guest, native, guestOffset, nativeOffset);
            Require(source.pixels == before);
        }
        Require(api.doneBitmap(library, &guest) == 0 && FT_Bitmap_Done(nativeLibrary, &native) == 0);
        Require(guest.buffer == nullptr && guest.rows == 0 && guest.width == 0);
    }
}

static void CheckErrors(const Api& api, FreeType::GuestLibrary* library, FT_Library nativeLibrary) {
    const Color color{1, 2, 3, 255};
    const FT_Color nativeColor{1, 2, 3, 255};
    Source source = MakeSource(FT_PIXEL_MODE_GRAY, 4, 4, false, 77);
    GuestBitmap empty{};
    GuestVector offset{5, 6};
    Require(api.blend(nullptr, &source.guest, {}, &empty, &offset, color) == FT_Err_Invalid_Argument);
    Require(api.blend(library, nullptr, {}, &empty, &offset, color) == FT_Err_Invalid_Argument);
    Require(api.blend(library, &source.guest, {}, nullptr, &offset, color) == FT_Err_Invalid_Argument);
    Require(api.blend(library, &source.guest, {}, &empty, nullptr, color) == FT_Err_Invalid_Argument);
    Require(empty.buffer == nullptr && empty.pixel_mode == 0 && offset.x == 5 && offset.y == 6);

    GuestBitmap target{};
    FT_Bitmap native;
    FT_Bitmap_Init(&native);
    GuestVector guestOffset{};
    FT_Vector nativeOffset{};
    const FT_Bitmap nativeSource = source.Native();
    Require(api.blend(library, &source.guest, {64, 64}, &target, &guestOffset, color) == 0);
    Require(FT_Bitmap_Blend(nativeLibrary, &nativeSource, {64, 64}, &native, &nativeOffset, nativeColor) == 0);
    CheckSame(target, native, guestOffset, nativeOffset);

    struct Case {
        Source source;
        GuestVector offset;
        GuestVector targetOffset;
        unsigned char targetMode;
        bool flipTargetPitch;
    };
    std::vector<Case> cases;
    cases.push_back({MakeSource(FT_PIXEL_MODE_NONE, 4, 4, false, 3), {0, 0}, guestOffset, FT_PIXEL_MODE_BGRA, false});
    cases.push_back({MakeSource(FT_PIXEL_MODE_GRAY, 0, 4, false, 4), {0, 0}, guestOffset, FT_PIXEL_MODE_BGRA, false});
    cases.push_back({MakeSource(FT_PIXEL_MODE_GRAY, 3, 0, false, 6), {0, 0}, guestOffset, FT_PIXEL_MODE_BGRA, false});
    cases.push_back({MakeSource(FT_PIXEL_MODE_GRAY, 2, 2, true, 8), {0, 0}, guestOffset, FT_PIXEL_MODE_BGRA, false});
    cases.push_back({MakeSource(FT_PIXEL_MODE_GRAY, 2, 2, false, 9), {0, 0}, guestOffset, FT_PIXEL_MODE_GRAY, false});
    cases.push_back({MakeSource(FT_PIXEL_MODE_GRAY, 2, 2, false, 11), {0x20000 * 64, 0}, guestOffset, FT_PIXEL_MODE_BGRA, false});
    cases.push_back({MakeSource(FT_PIXEL_MODE_GRAY, 2, 2, false, 12), {0, -0x20000 * 64}, guestOffset, FT_PIXEL_MODE_BGRA, false});
    cases.push_back({MakeSource(FT_PIXEL_MODE_GRAY, 2, 2, false, 13), {0, 0}, {-0x20000 * 64, 0}, FT_PIXEL_MODE_BGRA, false});
    cases.push_back({MakeSource(FT_PIXEL_MODE_GRAY, 2, 2, true, 14), {64, 0}, guestOffset, FT_PIXEL_MODE_BGRA, true});
    Source huge = MakeSource(FT_PIXEL_MODE_GRAY, 1, 1, false, 15);
    huge.guest.width = 0x8000;
    cases.push_back({std::move(huge), {0, 0}, guestOffset, FT_PIXEL_MODE_BGRA, false});
    for (auto& item : cases) {
        GuestBitmap guestTarget = target;
        FT_Bitmap nativeTarget = native;
        guestTarget.pixel_mode = item.targetMode;
        nativeTarget.pixel_mode = item.targetMode;
        std::vector<unsigned char> guestPixels = Pixels(target.buffer, target.rows, target.pitch);
        std::vector<unsigned char> nativePixels = guestPixels;
        guestTarget.buffer = guestPixels.data();
        nativeTarget.buffer = nativePixels.data();
        if (item.flipTargetPitch) {
            guestTarget.pitch = -guestTarget.pitch;
            nativeTarget.pitch = -nativeTarget.pitch;
        }
        GuestVector itemOffset = item.targetOffset;
        FT_Vector itemNativeOffset{static_cast<FT_Pos>(item.targetOffset.x), static_cast<FT_Pos>(item.targetOffset.y)};
        const FT_Bitmap itemSource = item.source.Native();
        const int error = api.blend(library, &item.source.guest, item.offset, &guestTarget, &itemOffset, color);
        Require(error == FT_Bitmap_Blend(nativeLibrary, &itemSource, {static_cast<FT_Pos>(item.offset.x), static_cast<FT_Pos>(item.offset.y)}, &nativeTarget,
                                         &itemNativeOffset, nativeColor));
        Require(guestTarget.buffer == guestPixels.data() && nativeTarget.buffer == nativePixels.data());
        Require(guestTarget.rows == nativeTarget.rows && guestTarget.width == nativeTarget.width && guestTarget.pitch == nativeTarget.pitch);
        Require(guestTarget.pixel_mode == nativeTarget.pixel_mode && guestPixels == nativePixels);
        Require(itemOffset.x == itemNativeOffset.x && itemOffset.y == itemNativeOffset.y);
        if (&item != &cases[8]) Require(guestPixels == Pixels(target.buffer, target.rows, target.pitch));
    }
    Require(api.doneBitmap(library, &target) == 0 && FT_Bitmap_Done(nativeLibrary, &native) == 0);
}

static void CheckInPlace(const Api& api, FreeType::GuestLibrary* library, FT_Library nativeLibrary, GuestBitmap* guestTarget, GuestVector* guestOffset) {
    const Source source = MakeSource(FT_PIXEL_MODE_GRAY, 3, 4, false, 31);
    const FT_Bitmap nativeSource = source.Native();
    std::vector<unsigned char> nativePixels = Pixels(guestTarget->buffer, guestTarget->rows, guestTarget->pitch);
    FT_Bitmap native{guestTarget->rows, guestTarget->width, guestTarget->pitch, nativePixels.data(), guestTarget->num_grays, guestTarget->pixel_mode, 0, nullptr};
    FT_Vector nativeOffset{static_cast<FT_Pos>(guestOffset->x), static_cast<FT_Pos>(guestOffset->y)};
    unsigned char* const buffer = guestTarget->buffer;
    Require(api.blend(library, &source.guest, {128, 192}, guestTarget, guestOffset, {40, 80, 120, 200}) == 0);
    Require(FT_Bitmap_Blend(nativeLibrary, &nativeSource, {128, 192}, &native, &nativeOffset, {40, 80, 120, 200}) == 0);
    Require(guestTarget->buffer == buffer && native.buffer == nativePixels.data());
    Require(guestTarget->rows == native.rows && guestTarget->width == native.width && guestTarget->pitch == native.pitch);
    Require(Pixels(guestTarget->buffer, guestTarget->rows, guestTarget->pitch) == nativePixels);
    Require(guestOffset->x == nativeOffset.x && guestOffset->y == nativeOffset.y);
}

static void CheckInPlaceHost(const Api& api, FreeType::GuestLibrary* library, FT_Library nativeLibrary) {
    std::vector<unsigned char> pixels(8 * 8 * 4, 0x40);
    GuestBitmap target{8, 8, 32, pixels.data(), 256, FT_PIXEL_MODE_BGRA, 0, nullptr};
    GuestVector offset{0, 512};
    CheckInPlace(api, library, nativeLibrary, &target, &offset);
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
        Require(api.allocate(0, 0x7fffffffffll, Page * 3, 0, 0, &physical) == 0);
        Require(api.map(&mapping, Page * 3, 3, 0, physical, 0) == 0);
        std::memset(mapping, 0xA5, Page * 3);
    }

    ~GuestPages() {
        Require(api.unmap(reinterpret_cast<std::uint64_t>(mapping), Page * 3) == 0);
        Require(api.release(physical, Page * 3) == 0);
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
        return {bytes, bytes + Page * 3};
    }

private:
    const KernelApi& api;
    std::int64_t physical = 0;
    void* mapping = nullptr;
};

static void CheckGuestMemory(const Api& api, const KernelApi& kernel, FreeType::GuestLibrary* library, FT_Library nativeLibrary) {
    GuestPages pages(kernel);
    auto* target = pages.At<GuestBitmap>(0);
    auto* offset = pages.At<GuestVector>(1);
    auto* pixels = pages.At<unsigned char>(2);
    std::memset(pixels, 0x40, 8 * 8 * 4);
    *target = {8, 8, 32, pixels, 256, FT_PIXEL_MODE_BGRA, 0, nullptr};
    *offset = {0, 512};
    const Source source = MakeSource(FT_PIXEL_MODE_GRAY, 3, 4, false, 31);
    for (unsigned page = 0; page < 3; ++page) {
        pages.Protect(page, 1);
        const auto before = pages.Snapshot();
        Require(api.blend(library, &source.guest, {128, 192}, target, offset, {40, 80, 120, 200}) == FT_Err_Invalid_Argument);
        Require(pages.Snapshot() == before);
        pages.Protect(page, 3);
    }
    pages.Protect(2, 1);
    GuestBitmap host{};
    GuestVector hostOffset{};
    const Source readOnlySource = [&] {
        Source copy = MakeSource(FT_PIXEL_MODE_BGRA, 2, 2, false, 41);
        copy.guest.buffer = pixels;
        copy.guest.pitch = 32;
        return copy;
    }();
    const auto readOnly = pages.Snapshot();
    Require(api.blend(library, &readOnlySource.guest, {0, 0}, &host, &hostOffset, {9, 9, 9, 255}) == 0 && pages.Snapshot() == readOnly);
    Require(api.doneBitmap(library, &host) == 0);
    pages.Protect(2, 3);
    CheckInPlace(api, library, nativeLibrary, target, offset);
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
    api.doneBitmap = Import<decltype(api.doneBitmap)>(base, patched ? "Mu2upxmY2C8" : "FT_Bitmap_Done_nid_postfix");
    api.blend = Import<decltype(api.blend)>(module, patched ? "C3-qsnriefE" : "C3-qsnriefE_nid_no_patch_cut");
    FreeType::GuestLibrary* library = nullptr;
    FT_Library nativeLibrary = nullptr;
    Require(api.initialize(&library) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    CheckSequences(api, library, nativeLibrary);
    CheckErrors(api, library, nativeLibrary);
    CheckInPlaceHost(api, library, nativeLibrary);
#ifdef _WIN32
    CheckGuestMemory(api, kernelApi, library, nativeLibrary);
#endif
    Require(api.doneLibrary(library) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
#ifdef _WIN32
    Require(FreeLibrary(base) != 0 && FreeLibrary(module) != 0 && FreeLibrary(kernel) != 0);
#else
    Require(dlclose(base) == 0 && dlclose(module) == 0);
#endif
}
