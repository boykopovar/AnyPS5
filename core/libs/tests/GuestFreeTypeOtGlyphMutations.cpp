#include "FreeTypeFontFixture.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_STROKER_H

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using FreeType::GuestGlyphRec;
using FreeType::GuestLibrary;
using FreeType::GuestMatrix;
using FreeType::GuestVector;

namespace {

using ToBitmap = FT_Error(APS5_VABI*)(GuestGlyphRec**, FT_Render_Mode, const GuestVector*, FT_Bool);
using Transform = FT_Error(APS5_VABI*)(GuestGlyphRec*, const GuestMatrix*, const GuestVector*);
using Stroke = FT_Error(APS5_VABI*)(GuestGlyphRec**, void*, FT_Bool);
using StrokeBorder = FT_Error(APS5_VABI*)(GuestGlyphRec**, void*, FT_Bool, FT_Bool);

struct Exports {
    using InitFreeType = decltype(&FT_Init_FreeType_nid_postfix);
    using DoneFreeType = decltype(&FT_Done_FreeType_nid_postfix);
    using NewMemoryFace = decltype(&FT_New_Memory_Face_nid_postfix);
    using DoneFace = decltype(&FT_Done_Face_nid_postfix);
    using SetPixelSizes = decltype(&FT_Set_Pixel_Sizes_nid_postfix);
    using LoadGlyph = decltype(&FT_Load_Glyph_nid_postfix);
    using GetGlyph = decltype(&FT_Get_Glyph_nid_postfix);
    using DoneGlyph = decltype(&FT_Done_Glyph_nid_postfix);
    using StrokerNew = decltype(&FT_Stroker_New_nid_postfix);
    using StrokerSet = decltype(&FT_Stroker_Set_nid_postfix);
    using StrokerDone = decltype(&FT_Stroker_Done_nid_postfix);

    InitFreeType initFreeType = nullptr;
    DoneFreeType doneFreeType = nullptr;
    NewMemoryFace newMemoryFace = nullptr;
    DoneFace doneFace = nullptr;
    SetPixelSizes setPixelSizes = nullptr;
    LoadGlyph loadGlyph = nullptr;
    GetGlyph getGlyph = nullptr;
    DoneGlyph doneGlyph = nullptr;
    StrokerNew strokerNew = nullptr;
    StrokerSet strokerSet = nullptr;
    StrokerDone strokerDone = nullptr;
    ToBitmap toBitmap = nullptr;
    Transform transform = nullptr;
    Stroke stroke = nullptr;
    StrokeBorder strokeBorder = nullptr;
};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FreeType Ot glyph mutations: %s\n", message);
    std::exit(1);
}

template <typename Function> void Resolve(Function& function, void* library, bool patched, const char* nid, const char* name) {
    const char* symbol = patched ? nid : name;
#ifdef _WIN32
    function = reinterpret_cast<Function>(GetProcAddress(static_cast<HMODULE>(library), symbol));
#else
    function = reinterpret_cast<Function>(dlsym(library, symbol));
#endif
    if (!function) std::fprintf(stderr, "FreeType Ot glyph mutations: missing %s\n", symbol);
    Require(function != nullptr, "missing export");
}

struct Fixture {
    std::vector<unsigned char> font = SquareGlyphFont('A', 700);
    GuestLibrary* library = nullptr;
    FreeType::GuestFaceRec* face = nullptr;
    FT_Library nativeLibrary = nullptr;
    FT_Face nativeFace = nullptr;
    const Exports& api;

    explicit Fixture(const Exports& exports) : api(exports) {
        Require(api.initFreeType(&library) == 0, "initialize guest library");
        Require(FT_Init_FreeType(&nativeLibrary) == 0, "initialize native library");
        Require(api.newMemoryFace(library, font.data(), static_cast<std::int64_t>(font.size()), 0, &face) == 0,
                "create guest face");
        Require(FT_New_Memory_Face(nativeLibrary, font.data(), static_cast<FT_Long>(font.size()), 0, &nativeFace) == 0,
                "create native face");
        Require(api.setPixelSizes(face, 0, 48) == FT_Set_Pixel_Sizes(nativeFace, 0, 48), "set face size");
    }

    std::pair<GuestGlyphRec*, FT_Glyph> Glyph() {
        Require(api.loadGlyph(face, 1, FT_LOAD_NO_HINTING) == 0, "load guest glyph");
        Require(FT_Load_Glyph(nativeFace, 1, FT_LOAD_NO_HINTING) == 0, "load native glyph");
        GuestGlyphRec* guest = nullptr;
        FT_Glyph native = nullptr;
        Require(api.getGlyph(face->glyph, &guest) == FT_Get_Glyph(nativeFace->glyph, &native), "create glyph pair");
        Require(guest && native, "glyph pair exists");
        return {guest, native};
    }

    ~Fixture() {
        if (face) Require(api.doneFace(face) == 0, "destroy guest face");
        if (nativeFace) Require(FT_Done_Face(nativeFace) == 0, "destroy native face");
        if (library) Require(api.doneFreeType(library) == 0, "destroy guest library");
        if (nativeLibrary) Require(FT_Done_FreeType(nativeLibrary) == 0, "destroy native library");
    }
};

void Matches(const GuestGlyphRec* guest, FT_Glyph native, const GuestLibrary* library, bool bitmap) {
    Require(guest && native && guest->library == library && guest->clazz, "glyph roots are valid");
    Require(guest->format == static_cast<std::uint32_t>(native->format), "glyph formats match");
    Require(guest->advance.x == native->advance.x && guest->advance.y == native->advance.y, "glyph advances match");
    if (bitmap) {
        const auto& left = *reinterpret_cast<const FreeType::GuestBitmapGlyphRec*>(guest);
        const auto right = reinterpret_cast<FT_BitmapGlyph>(native);
        Require(left.left == right->left && left.top == right->top, "bitmap bearings match");
        const auto& a = left.bitmap;
        const auto& b = right->bitmap;
        Require(a.rows == b.rows && a.width == b.width && a.pitch == b.pitch && a.num_grays == b.num_grays &&
                    a.pixel_mode == b.pixel_mode && a.palette_mode == b.palette_mode && a.palette == b.palette,
                "bitmap metadata matches");
        const auto pitch = a.pitch < 0 ? -static_cast<std::int64_t>(a.pitch) : a.pitch;
        const auto bytes = static_cast<std::size_t>(a.rows) * static_cast<std::size_t>(pitch);
        Require((a.buffer == nullptr) == (b.buffer == nullptr), "bitmap buffer presence matches");
        if (bytes && a.buffer) Require(std::memcmp(a.buffer, b.buffer, bytes) == 0, "bitmap pixels match");
        return;
    }
    const auto& a = reinterpret_cast<const FreeType::GuestOutlineGlyphRec*>(guest)->outline;
    const auto& b = reinterpret_cast<FT_OutlineGlyph>(native)->outline;
    Require(a.n_points == b.n_points && a.n_contours == b.n_contours && a.flags == b.flags, "outline metadata matches");
    for (std::uint32_t index = 0; index < a.n_points; ++index) {
        Require(a.points[index].x == b.points[index].x && a.points[index].y == b.points[index].y && a.tags[index] == b.tags[index],
                "outline points match");
    }
    for (std::uint32_t index = 0; index < a.n_contours; ++index) Require(a.contours[index] == b.contours[index], "outline contours match");
}

void EditGuestOutline(GuestGlyphRec* guest) {
    auto& guestOutline = reinterpret_cast<FreeType::GuestOutlineGlyphRec*>(guest)->outline;
    guestOutline.points[0].x += 137;
    guestOutline.points[0].y -= 29;
    guestOutline.flags ^= FT_OUTLINE_EVEN_ODD_FILL;
    guest->advance.x += 11;
}

void EditNativeOutline(FT_Glyph native) {
    auto& nativeOutline = reinterpret_cast<FT_OutlineGlyph>(native)->outline;
    nativeOutline.points[0].x += 137;
    nativeOutline.points[0].y -= 29;
    nativeOutline.flags ^= FT_OUTLINE_EVEN_ODD_FILL;
    native->advance.x += 11;
}

struct StrokerPair {
    void* guest = nullptr;
    FT_Stroker native = nullptr;
    const Exports& api;

    explicit StrokerPair(Fixture& fixture) : api(fixture.api) {
        Require(api.strokerNew(fixture.library, &guest) == FT_Stroker_New(fixture.nativeLibrary, &native), "create stroker pair");
        api.strokerSet(guest, 96, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0x10000);
        FT_Stroker_Set(native, 96, FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0x10000);
    }

    ~StrokerPair() {
        api.strokerDone(guest);
        FT_Stroker_Done(native);
    }
};

void BitmapConversion(const Exports& ot, FT_Bool destroy) {
    Fixture fixture(ot);
    auto [guest, native] = fixture.Glyph();
    EditGuestOutline(guest);
    EditNativeOutline(native);
    const auto* oldGuest = guest;
    const FT_Glyph oldNative = native;
    const GuestVector origin{17, -9};
    const FT_Vector nativeOrigin{17, -9};
    try {
        Require(ot.toBitmap(&guest, FT_RENDER_MODE_NORMAL, &origin, destroy) ==
                    FT_Glyph_To_Bitmap(&native, FT_RENDER_MODE_NORMAL, &nativeOrigin, destroy),
                "bitmap conversion error matches FreeType");
    } catch (...) {
        if (!destroy && guest != oldGuest) ot.doneGlyph(const_cast<GuestGlyphRec*>(oldGuest));
        if (guest) ot.doneGlyph(guest);
        if (!destroy && native != oldNative) FT_Done_Glyph(oldNative);
        if (native) FT_Done_Glyph(native);
        throw;
    }
    Matches(guest, native, fixture.library, true);
    if (!destroy) {
        Matches(oldGuest, oldNative, fixture.library, false);
        ot.doneGlyph(const_cast<GuestGlyphRec*>(oldGuest));
        FT_Done_Glyph(oldNative);
    }
    ot.doneGlyph(guest);
    FT_Done_Glyph(native);
}

void TransformGlyph(const Exports& ot) {
    Fixture fixture(ot);
    auto [guest, native] = fixture.Glyph();
    EditGuestOutline(guest);
    EditNativeOutline(native);
    const GuestMatrix matrix{0x18000, 0x08000, -0x04000, 0x10000};
    const FT_Matrix nativeMatrix{0x18000, 0x08000, -0x04000, 0x10000};
    const GuestVector delta{137, -211};
    const FT_Vector nativeDelta{137, -211};
    Require(ot.transform(guest, &matrix, &delta) == FT_Glyph_Transform(native, &nativeMatrix, &nativeDelta),
            "transform error matches FreeType");
    Matches(guest, native, fixture.library, false);
    ot.doneGlyph(guest);
    FT_Done_Glyph(native);
}

void TransformOptional(const Exports& ot, bool useMatrix, bool useDelta) {
    Fixture fixture(ot);
    auto [guest, native] = fixture.Glyph();
    EditGuestOutline(guest);
    EditNativeOutline(native);
    const GuestMatrix matrix{0x10000, 0x04000, -0x02000, 0x10000};
    const FT_Matrix nativeMatrix{0x10000, 0x04000, -0x02000, 0x10000};
    const GuestVector delta{23, -31};
    const FT_Vector nativeDelta{23, -31};
    Require(ot.transform(guest, useMatrix ? &matrix : nullptr, useDelta ? &delta : nullptr) ==
                FT_Glyph_Transform(native, useMatrix ? &nativeMatrix : nullptr, useDelta ? &nativeDelta : nullptr),
            "optional transform arguments match FreeType");
    Matches(guest, native, fixture.library, false);
    ot.doneGlyph(guest);
    FT_Done_Glyph(native);
}

void StrokeGlyph(const Exports& ot, FT_Bool destroy) {
    Fixture fixture(ot);
    StrokerPair stroker(fixture);
    auto [guest, native] = fixture.Glyph();
    EditGuestOutline(guest);
    EditNativeOutline(native);
    auto* oldGuest = guest;
    const FT_Glyph oldNative = native;
    Require(ot.stroke(&guest, stroker.guest, destroy) == FT_Glyph_Stroke(&native, stroker.native, destroy), "stroke error matches FreeType");
    Matches(guest, native, fixture.library, false);
    if (!destroy) {
        Matches(oldGuest, oldNative, fixture.library, false);
        ot.doneGlyph(oldGuest);
        FT_Done_Glyph(oldNative);
    }
    ot.doneGlyph(guest);
    FT_Done_Glyph(native);
}

void StrokeBorderGlyph(const Exports& ot, FT_Bool inside, FT_Bool destroy) {
    Fixture fixture(ot);
    StrokerPair stroker(fixture);
    auto [guest, native] = fixture.Glyph();
    EditGuestOutline(guest);
    EditNativeOutline(native);
    auto* oldGuest = guest;
    const FT_Glyph oldNative = native;
    Require(ot.strokeBorder(&guest, stroker.guest, inside, destroy) == FT_Glyph_StrokeBorder(&native, stroker.native, inside, destroy),
            "border stroke error matches FreeType");
    Matches(guest, native, fixture.library, false);
    if (!destroy) {
        Matches(oldGuest, oldNative, fixture.library, false);
        ot.doneGlyph(oldGuest);
        FT_Done_Glyph(oldNative);
    }
    ot.doneGlyph(guest);
    FT_Done_Glyph(native);
}

void InvalidBitmapStroke(const Exports& ot) {
    Fixture fixture(ot);
    StrokerPair stroker(fixture);
    auto [guest, native] = fixture.Glyph();
    Require(ot.toBitmap(&guest, FT_RENDER_MODE_NORMAL, nullptr, 1) == FT_Glyph_To_Bitmap(&native, FT_RENDER_MODE_NORMAL, nullptr, 1),
            "setup bitmap conversion matches FreeType");
    auto& guestBitmap = *reinterpret_cast<FreeType::GuestBitmapGlyphRec*>(guest);
    auto nativeBitmap = reinterpret_cast<FT_BitmapGlyph>(native);
    guestBitmap.left += 2;
    guestBitmap.top -= 3;
    guest->advance.x += 13;
    nativeBitmap->left += 2;
    nativeBitmap->top -= 3;
    native->advance.x += 13;
    Require(ot.toBitmap(&guest, FT_RENDER_MODE_NORMAL, nullptr, 1) == FT_Glyph_To_Bitmap(&native, FT_RENDER_MODE_NORMAL, nullptr, 1),
            "existing bitmap conversion matches FreeType");
    Matches(guest, native, fixture.library, true);
    auto* oldGuest = guest;
    const FT_Glyph oldNative = native;
    const auto guestError = ot.stroke(&guest, stroker.guest, 1);
    const auto nativeError = FT_Glyph_Stroke(&native, stroker.native, 1);
    Require(guestError == nativeError && guest == oldGuest && native == oldNative, "invalid bitmap stroke preserves input and matches error");
    Matches(guest, native, fixture.library, true);
    ot.doneGlyph(guest);
    FT_Done_Glyph(native);
}

void InvalidHandles(const Exports& ot) {
    GuestGlyphRec* guest = nullptr;
    FT_Glyph native = nullptr;
    Require(ot.toBitmap(&guest, FT_RENDER_MODE_NORMAL, nullptr, 1) == FT_Glyph_To_Bitmap(&native, FT_RENDER_MODE_NORMAL, nullptr, 1),
            "null bitmap conversion matches FreeType");
    Require(ot.transform(nullptr, nullptr, nullptr) == FT_Glyph_Transform(nullptr, nullptr, nullptr), "null transform matches FreeType");
    Require(ot.stroke(&guest, nullptr, 1) == FT_Glyph_Stroke(&native, nullptr, 1), "null glyph stroke matches FreeType");
    Require(ot.strokeBorder(&guest, nullptr, 1, 1) == FT_Glyph_StrokeBorder(&native, nullptr, 1, 1),
            "null glyph border stroke matches FreeType");
    GuestGlyphRec foreign{};
    bool threw = false;
    try {
        ot.transform(&foreign, nullptr, nullptr);
    } catch (const std::runtime_error& error) {
        threw = std::string(error.what()).find("sceFreeTypeOtGlyphTransform_nid_no_patch") != std::string::npos;
    }
    Require(threw, "foreign glyph handle is not accepted");
}

void InvalidOutlinePointerPreservesOwner(const Exports& ot) {
    Fixture fixture(ot);
    auto [guest, native] = fixture.Glyph();
    auto& outline = reinterpret_cast<FreeType::GuestOutlineGlyphRec*>(guest)->outline;
    auto* originalPoints = outline.points;
    GuestVector foreignPoint{};
    outline.points = &foreignPoint;
    bool threw = false;
    try {
        ot.transform(guest, nullptr, nullptr);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    outline.points = originalPoints;
    Require(threw, "rebound outline point storage is rejected");
    Require(ot.transform(guest, nullptr, nullptr) == FT_Glyph_Transform(native, nullptr, nullptr),
            "original owner survives rejected point storage");
    Matches(guest, native, fixture.library, false);
    ot.doneGlyph(guest);
    FT_Done_Glyph(native);
}

}

struct LoadedLibraries {
    void* freeType = nullptr;
    void* ot = nullptr;

    ~LoadedLibraries() {
#ifdef _WIN32
        if (ot) FreeLibrary(static_cast<HMODULE>(ot));
        if (freeType) FreeLibrary(static_cast<HMODULE>(freeType));
#else
        if (ot) dlclose(ot);
        if (freeType) dlclose(freeType);
#endif
    }
};

int main(int argc, char** argv) {
    Require(argc == 4, "expected FreeType provider, OT provider, and export mode");
    const bool patched = std::strcmp(argv[3], "patched") == 0;
    Require(patched || std::strcmp(argv[3], "raw") == 0, "expected raw or patched export mode");
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    LoadedLibraries libraries;
#ifdef _WIN32
    libraries.freeType = LoadLibraryA(argv[1]);
    libraries.ot = LoadLibraryA(argv[2]);
#else
    libraries.freeType = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    libraries.ot = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
#endif
    Require(libraries.freeType != nullptr, "load FreeType provider");
    Require(libraries.ot != nullptr, "load OT provider");
    Exports ot;
    Resolve(ot.initFreeType, libraries.freeType, patched, "GNsiLzm4SH8", "FT_Init_FreeType_nid_postfix");
    Resolve(ot.doneFreeType, libraries.freeType, patched, "dH02CpEEcYk", "FT_Done_FreeType_nid_postfix");
    Resolve(ot.newMemoryFace, libraries.freeType, patched, "40Wa2neSLM0", "FT_New_Memory_Face_nid_postfix");
    Resolve(ot.doneFace, libraries.freeType, patched, "OgFczHko0w8", "FT_Done_Face_nid_postfix");
    Resolve(ot.setPixelSizes, libraries.freeType, patched, "lJxMIwj81SQ", "FT_Set_Pixel_Sizes_nid_postfix");
    Resolve(ot.loadGlyph, libraries.freeType, patched, "M156RuYoUbU", "FT_Load_Glyph_nid_postfix");
    Resolve(ot.getGlyph, libraries.freeType, patched, "eMKHL+Cj50k", "FT_Get_Glyph_nid_postfix");
    Resolve(ot.doneGlyph, libraries.freeType, patched, "RdrrejUMiJ0", "FT_Done_Glyph_nid_postfix");
    Resolve(ot.strokerNew, libraries.freeType, patched, "eqGCaeizvWM", "FT_Stroker_New_nid_postfix");
    Resolve(ot.strokerSet, libraries.freeType, patched, "B3uU5aDZn94", "FT_Stroker_Set_nid_postfix");
    Resolve(ot.strokerDone, libraries.freeType, patched, "+RLa-6fbshA", "FT_Stroker_Done_nid_postfix");
    Resolve(ot.toBitmap, libraries.ot, patched, "c1nbr8oSWlg", "sceFreeTypeOtGlyphToBitmap_nid_no_patch");
    Resolve(ot.transform, libraries.ot, patched, "AVEEb57HhUw", "sceFreeTypeOtGlyphTransform_nid_no_patch");
    Resolve(ot.stroke, libraries.ot, patched, "0rKmI1ZbMAc", "FT_Glyph_Stroke_nid_postfix");
    Resolve(ot.strokeBorder, libraries.ot, patched, "BBRhYfYBDgU", "sceFreeTypeOtGlyphStrokeBorder_nid_no_patch");
    try {
        BitmapConversion(ot, 0);
        BitmapConversion(ot, 1);
        BitmapConversion(ot, 2);
        TransformGlyph(ot);
        TransformOptional(ot, false, false);
        TransformOptional(ot, false, true);
        TransformOptional(ot, true, false);
        StrokeGlyph(ot, 0);
        StrokeGlyph(ot, 1);
        StrokeGlyph(ot, 2);
        for (const auto inside : {FT_Bool{0}, FT_Bool{1}}) {
            StrokeBorderGlyph(ot, inside, 0);
            StrokeBorderGlyph(ot, inside, 1);
        }
        StrokeBorderGlyph(ot, 2, 2);
        InvalidBitmapStroke(ot);
        InvalidHandles(ot);
        InvalidOutlinePointerPreservesOwner(ot);
    } catch (const std::runtime_error& error) {
        std::fprintf(stderr, "FreeType Ot glyph mutations: %s\n", error.what());
        return 1;
    }
    std::puts("FreeType Ot glyph mutations match FreeType");
    return 0;
}
