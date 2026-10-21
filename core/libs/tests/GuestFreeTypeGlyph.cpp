#include "FreeTypeFontFixture.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

using namespace FreeType;

extern "C" {
int APS5_VABI FT_Init_FreeType_nid_postfix(void**);
int APS5_VABI FT_Done_FreeType_nid_postfix(void*);
int APS5_VABI FT_New_Memory_Face_nid_postfix(void*, const unsigned char*, std::int64_t, std::int64_t, GuestFaceRec**);
int APS5_VABI FT_Done_Face_nid_postfix(GuestFaceRec*);
int APS5_VABI FT_Set_Pixel_Sizes_nid_postfix(GuestFaceRec*, std::uint32_t, std::uint32_t);
int APS5_VABI FT_Load_Glyph_nid_postfix(GuestFaceRec*, std::uint32_t, std::int32_t);
int APS5_VABI FT_Get_Glyph_nid_postfix(GuestGlyphSlotRec*, GuestGlyphRec**);
int APS5_VABI FT_Glyph_Copy_nid_postfix(GuestGlyphRec*, GuestGlyphRec**);
void APS5_VABI FT_Done_Glyph_nid_postfix(GuestGlyphRec*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType glyph check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename TCall> static bool Throws(TCall call, const char* function) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()).find(function) != std::string::npos;
    }
    return false;
}

static void Matches(const GuestGlyphRec* glyph, FT_Glyph native, void* library, bool isBitmap) {
    Require(glyph && native && glyph->library == library && glyph->clazz);
    Require(glyph->format == static_cast<std::uint32_t>(native->format));
    Require(glyph->advance.x == native->advance.x && glyph->advance.y == native->advance.y);
    if (isBitmap) {
        const auto& image = *reinterpret_cast<const GuestBitmapGlyphRec*>(glyph);
        const auto host = reinterpret_cast<FT_BitmapGlyph>(native);
        Require(image.left == host->left && image.top == host->top);
        const auto& a = image.bitmap;
        const auto& b = host->bitmap;
        Require(a.rows == b.rows && a.width == b.width && a.pitch == b.pitch);
        Require(a.num_grays == b.num_grays && a.pixel_mode == b.pixel_mode && a.palette_mode == b.palette_mode && a.palette == b.palette);
        Require((a.buffer == nullptr) == (b.buffer == nullptr));
        const std::size_t size = a.rows * static_cast<std::size_t>(a.pitch < 0 ? -static_cast<std::int64_t>(a.pitch) : a.pitch);
        if (size && a.buffer) Require(std::memcmp(a.buffer, b.buffer, size) == 0);
    } else {
        const auto& a = reinterpret_cast<const GuestOutlineGlyphRec*>(glyph)->outline;
        const auto& b = reinterpret_cast<FT_OutlineGlyph>(native)->outline;
        Require(a.n_points == b.n_points && a.n_contours == b.n_contours && a.flags == b.flags);
        for (unsigned index = 0; index < a.n_points; ++index) {
            Require(a.points[index].x == b.points[index].x && a.points[index].y == b.points[index].y && a.tags[index] == static_cast<unsigned char>(b.tags[index]));
        }
        for (unsigned index = 0; index < a.n_contours; ++index) Require(a.contours[index] == static_cast<std::uint16_t>(b.contours[index]));
    }
}

struct Fixture {
    std::vector<unsigned char> font = SquareGlyphFont('A', 700);
    void* library = nullptr;
    GuestFaceRec* face = nullptr;
    FT_Library hostLibrary = nullptr;
    FT_Face host = nullptr;

    Fixture() {
        Require(FT_Init_FreeType_nid_postfix(&library) == 0);
        Require(FT_Init_FreeType(&hostLibrary) == 0);
        Require(FT_New_Memory_Face_nid_postfix(library, font.data(), static_cast<std::int64_t>(font.size()), 0, &face) == 0);
        Require(FT_New_Memory_Face(hostLibrary, font.data(), static_cast<FT_Long>(font.size()), 0, &host) == 0);
        Require(FT_Set_Pixel_Sizes_nid_postfix(face, 0, 48) == FT_Set_Pixel_Sizes(host, 0, 48));
    }

    void Load(std::uint32_t index, std::int32_t flags) {
        Require(FT_Load_Glyph_nid_postfix(face, index, flags) == 0);
        Require(FT_Load_Glyph(host, index, flags) == 0);
    }

    void CloseFaces() {
        Require(FT_Done_Face_nid_postfix(face) == 0);
        Require(FT_Done_Face(host) == 0);
        face = nullptr;
        host = nullptr;
    }

    ~Fixture() {
        if (face) CloseFaces();
        Require(FT_Done_FreeType_nid_postfix(library) == 0);
        Require(FT_Done_FreeType(hostLibrary) == 0);
    }
};

static void OutlineOwnership() {
    Fixture fixture;
    fixture.Load(1, FT_LOAD_NO_HINTING);
    auto* slot = fixture.face->glyph;
    auto hostSlot = fixture.host->glyph;
    slot->advance = {-731, 2047};
    hostSlot->advance = {-731, 2047};
    slot->outline.points[0].x += 41;
    hostSlot->outline.points[0].x += 41;
    slot->outline.flags ^= FT_OUTLINE_EVEN_ODD_FILL;
    hostSlot->outline.flags ^= FT_OUTLINE_EVEN_ODD_FILL;
    slot->outline.tags[0] ^= FT_CURVE_TAG_TOUCH_X;
    hostSlot->outline.tags[0] ^= FT_CURVE_TAG_TOUCH_X;
    GuestGlyphRec* glyph = nullptr;
    FT_Glyph native = nullptr;
    Require(FT_Get_Glyph_nid_postfix(slot, &glyph) == FT_Get_Glyph(hostSlot, &native));
    Matches(glyph, native, fixture.library, false);
    auto& image = reinterpret_cast<GuestOutlineGlyphRec*>(glyph)->outline;
    auto& nativeImage = reinterpret_cast<FT_OutlineGlyph>(native)->outline;
    Require(image.points != slot->outline.points && image.tags != slot->outline.tags && image.contours != slot->outline.contours);
    Require(glyph->advance.x == -731 * 1024 && glyph->advance.y == 2047 * 1024);
    Require(slot->outline.points[0].x == image.points[0].x);
    fixture.Load(0, FT_LOAD_NO_HINTING);
    Matches(glyph, native, fixture.library, false);
    fixture.CloseFaces();
    Matches(glyph, native, fixture.library, false);
    image.points[1].y -= 19;
    nativeImage.points[1].y -= 19;
    image.tags[1] ^= FT_CURVE_TAG_TOUCH_Y;
    nativeImage.tags[1] ^= FT_CURVE_TAG_TOUCH_Y;
    image.flags &= ~FT_OUTLINE_OWNER;
    nativeImage.flags &= ~FT_OUTLINE_OWNER;
    glyph->format = static_cast<std::uint32_t>(FT_GLYPH_FORMAT_NONE);
    native->format = FT_GLYPH_FORMAT_NONE;
    glyph->advance = {-1234567, 987654};
    native->advance = {-1234567, 987654};
    GuestGlyphRec* copy = nullptr;
    FT_Glyph nativeCopy = nullptr;
    Require(FT_Glyph_Copy_nid_postfix(glyph, &copy) == FT_Glyph_Copy(native, &nativeCopy));
    Matches(copy, nativeCopy, fixture.library, false);
    auto& copiedImage = reinterpret_cast<GuestOutlineGlyphRec*>(copy)->outline;
    Require(copiedImage.points != image.points && copiedImage.tags != image.tags && copiedImage.contours != image.contours);
    Require((copiedImage.flags & FT_OUTLINE_OWNER) != 0);
    image.points[0].x += 100;
    image.tags[0] ^= FT_CURVE_TAG_TOUCH_Y;
    image.contours[0] = 0;
    Matches(copy, nativeCopy, fixture.library, false);
    image.flags |= FT_OUTLINE_OWNER;
    nativeImage.flags |= FT_OUTLINE_OWNER;
    FT_Done_Glyph_nid_postfix(glyph);
    FT_Done_Glyph(native);
    Matches(copy, nativeCopy, fixture.library, false);
    auto* original = copy;
    const FT_Glyph originalNative = nativeCopy;
    Require(FT_Glyph_Copy_nid_postfix(copy, &copy) == FT_Glyph_Copy(nativeCopy, &nativeCopy));
    Require(copy != original && nativeCopy != originalNative);
    FT_Done_Glyph_nid_postfix(original);
    FT_Done_Glyph(originalNative);
    Matches(copy, nativeCopy, fixture.library, false);
    FT_Done_Glyph_nid_postfix(copy);
    FT_Done_Glyph(nativeCopy);
}

static void BitmapOwnership() {
    Fixture fixture;
    fixture.Load(1, FT_LOAD_RENDER);
    auto* slot = fixture.face->glyph;
    const auto hostSlot = fixture.host->glyph;
    slot->bitmap_left = hostSlot->bitmap_left = -7;
    slot->bitmap_top = hostSlot->bitmap_top = 19;
    slot->advance = {-27, 53};
    hostSlot->advance = {-27, 53};
    GuestGlyphRec* glyph = nullptr;
    FT_Glyph native = nullptr;
    Require(FT_Get_Glyph_nid_postfix(slot, &glyph) == FT_Get_Glyph(hostSlot, &native));
    auto& image = *reinterpret_cast<GuestBitmapGlyphRec*>(glyph);
    const auto nativeImage = reinterpret_cast<FT_BitmapGlyph>(native);
    Require(image.bitmap.buffer == slot->bitmap.buffer && nativeImage->bitmap.buffer == hostSlot->bitmap.buffer);
    Matches(glyph, native, fixture.library, true);
    GuestGlyphRec* second = nullptr;
    FT_Glyph nativeSecond = nullptr;
    Require(FT_Get_Glyph_nid_postfix(slot, &second) == FT_Get_Glyph(hostSlot, &nativeSecond));
    Require(reinterpret_cast<GuestBitmapGlyphRec*>(second)->bitmap.buffer != image.bitmap.buffer);
    Require(reinterpret_cast<FT_BitmapGlyph>(nativeSecond)->bitmap.buffer != nativeImage->bitmap.buffer);
    Matches(second, nativeSecond, fixture.library, true);
    fixture.Load(0, FT_LOAD_RENDER);
    fixture.CloseFaces();
    Matches(glyph, native, fixture.library, true);
    FT_Done_Glyph_nid_postfix(second);
    FT_Done_Glyph(nativeSecond);
    for (const auto mode : {FT_PIXEL_MODE_MONO, FT_PIXEL_MODE_GRAY2, FT_PIXEL_MODE_GRAY4, FT_PIXEL_MODE_GRAY, FT_PIXEL_MODE_LCD, FT_PIXEL_MODE_LCD_V, FT_PIXEL_MODE_BGRA}) {
        for (const int pitch : {-24, 24}) {
            image.left = nativeImage->left = -13;
            image.top = nativeImage->top = 17;
            image.bitmap.rows = nativeImage->bitmap.rows = 3;
            image.bitmap.width = nativeImage->bitmap.width = 5;
            image.bitmap.pitch = nativeImage->bitmap.pitch = pitch;
            image.bitmap.pixel_mode = nativeImage->bitmap.pixel_mode = static_cast<unsigned char>(mode);
            GuestGlyphRec* copy = nullptr;
            FT_Glyph nativeCopy = nullptr;
            Require(FT_Glyph_Copy_nid_postfix(glyph, &copy) == FT_Glyph_Copy(native, &nativeCopy));
            Matches(copy, nativeCopy, fixture.library, true);
            const auto buffer = reinterpret_cast<GuestBitmapGlyphRec*>(copy)->bitmap.buffer;
            Require(buffer != image.bitmap.buffer);
            const auto oldPixel = buffer[0];
            image.bitmap.buffer[0] ^= 0xA5;
            nativeImage->bitmap.buffer[0] ^= 0xA5;
            Require(buffer[0] == oldPixel);
            Matches(copy, nativeCopy, fixture.library, true);
            FT_Done_Glyph_nid_postfix(copy);
            FT_Done_Glyph(nativeCopy);
        }
    }
    const auto savedBuffer = image.bitmap.buffer;
    const auto savedNativeBuffer = nativeImage->bitmap.buffer;
    image.bitmap.buffer = nullptr;
    nativeImage->bitmap.buffer = nullptr;
    GuestGlyphRec* empty = nullptr;
    FT_Glyph nativeEmpty = nullptr;
    Require(FT_Glyph_Copy_nid_postfix(glyph, &empty) == FT_Glyph_Copy(native, &nativeEmpty));
    Matches(empty, nativeEmpty, fixture.library, true);
    FT_Done_Glyph_nid_postfix(empty);
    FT_Done_Glyph(nativeEmpty);
    image.bitmap.buffer = savedBuffer;
    nativeImage->bitmap.buffer = savedNativeBuffer;
    GuestGlyphRec* retained = nullptr;
    FT_Glyph nativeRetained = nullptr;
    Require(FT_Glyph_Copy_nid_postfix(glyph, &retained) == FT_Glyph_Copy(native, &nativeRetained));
    FT_Done_Glyph_nid_postfix(glyph);
    FT_Done_Glyph(native);
    Matches(retained, nativeRetained, fixture.library, true);
    FT_Done_Glyph_nid_postfix(retained);
    FT_Done_Glyph(nativeRetained);
}

static void WideOutlineCoordinates() {
    Fixture fixture;
    fixture.Load(1, FT_LOAD_NO_HINTING);
    const std::array<GuestVector, 4> expected{{
        {std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max()},
        {std::numeric_limits<std::int64_t>::max(), std::numeric_limits<std::int64_t>::min()},
        {std::int64_t{1} << 40, -(std::int64_t{1} << 40)},
        {-2147483649LL, 2147483648LL},
    }};
    auto& slotOutline = fixture.face->glyph->outline;
    Require(slotOutline.n_points == expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        slotOutline.points[index] = expected[index];
        fixture.host->glyph->outline.points[index] = {static_cast<FT_Pos>(expected[index].x), static_cast<FT_Pos>(expected[index].y)};
    }
    GuestGlyphRec* glyph = nullptr;
    FT_Glyph native = nullptr;
    Require(FT_Get_Glyph_nid_postfix(fixture.face->glyph, &glyph) == FT_Get_Glyph(fixture.host->glyph, &native));
    auto& outline = reinterpret_cast<GuestOutlineGlyphRec*>(glyph)->outline;
    Require(outline.n_points == expected.size() && outline.points != slotOutline.points);
    for (std::size_t index = 0; index < expected.size(); ++index) {
        Require(outline.points[index].x == expected[index].x && outline.points[index].y == expected[index].y);
    }
    if constexpr (sizeof(FT_Pos) == sizeof(std::int64_t)) Matches(glyph, native, fixture.library, false);
    fixture.Load(0, FT_LOAD_NO_HINTING);
    fixture.CloseFaces();
    GuestGlyphRec* copy = nullptr;
    FT_Glyph nativeCopy = nullptr;
    Require(FT_Glyph_Copy_nid_postfix(glyph, &copy) == FT_Glyph_Copy(native, &nativeCopy));
    const auto& copiedOutline = reinterpret_cast<GuestOutlineGlyphRec*>(copy)->outline;
    Require(copiedOutline.points != outline.points && copiedOutline.n_points == expected.size());
    outline.points[0] = {};
    FT_Done_Glyph_nid_postfix(glyph);
    FT_Done_Glyph(native);
    for (std::size_t index = 0; index < expected.size(); ++index) {
        Require(copiedOutline.points[index].x == expected[index].x && copiedOutline.points[index].y == expected[index].y);
    }
    if constexpr (sizeof(FT_Pos) == sizeof(std::int64_t)) Matches(copy, nativeCopy, fixture.library, false);
    FT_Done_Glyph_nid_postfix(copy);
    FT_Done_Glyph(nativeCopy);
}

static void ErrorsAndEmptyGlyphs() {
    Fixture fixture;
    fixture.Load(1, FT_LOAD_NO_HINTING);
    auto* slot = fixture.face->glyph;
    const auto hostSlot = fixture.host->glyph;
    GuestGlyphRec sentinel{};
    FT_GlyphRec nativeSentinel{};
    GuestGlyphRec* glyph = &sentinel;
    FT_Glyph native = &nativeSentinel;
    Require(FT_Get_Glyph_nid_postfix(nullptr, &glyph) == FT_Get_Glyph(nullptr, &native));
    Require(glyph == &sentinel && native == &nativeSentinel);
    Require(FT_Get_Glyph_nid_postfix(nullptr, nullptr) == FT_Get_Glyph(nullptr, nullptr));
    Require(FT_Get_Glyph_nid_postfix(slot, nullptr) == FT_Get_Glyph(hostSlot, nullptr));
    for (const auto format : {FT_GLYPH_FORMAT_NONE, FT_GLYPH_FORMAT_COMPOSITE, static_cast<FT_Glyph_Format>(0x12345678)}) {
        slot->format = static_cast<std::uint32_t>(format);
        hostSlot->format = format;
        Require(FT_Get_Glyph_nid_postfix(slot, &glyph) == FT_Get_Glyph(hostSlot, &native));
        Require(glyph == &sentinel && native == &nativeSentinel);
    }
    slot->format = static_cast<std::uint32_t>(FT_GLYPH_FORMAT_OUTLINE);
    hostSlot->format = FT_GLYPH_FORMAT_OUTLINE;
    const auto library = slot->library;
    const auto hostLibrary = hostSlot->library;
    slot->library = nullptr;
    hostSlot->library = nullptr;
    Require(FT_Get_Glyph_nid_postfix(slot, &glyph) == FT_Get_Glyph(hostSlot, &native));
    Require(glyph == &sentinel && native == &nativeSentinel);
    slot->library = library;
    hostSlot->library = hostLibrary;
    for (const std::int64_t value : std::array<std::int64_t, 6>{-0x200000, 0x200000, -0x200001, 0x200001, std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max()}) {
        for (const bool vertical : {false, true}) {
            slot->advance = vertical ? GuestVector{0, value} : GuestVector{value, 0};
            const auto bounded = static_cast<FT_Pos>(value < 0 ? -0x200000 : 0x200000);
            hostSlot->advance = vertical ? FT_Vector{0, bounded} : FT_Vector{bounded, 0};
            glyph = &sentinel;
            native = &nativeSentinel;
            Require(FT_Get_Glyph_nid_postfix(slot, &glyph) == FT_Get_Glyph(hostSlot, &native));
            Require(glyph == nullptr && native == nullptr);
        }
    }
    for (const std::int64_t value : {-0x1FFFFF, 0x1FFFFF}) {
        slot->advance = {value, -value};
        hostSlot->advance = {static_cast<FT_Pos>(value), static_cast<FT_Pos>(-value)};
        Require(FT_Get_Glyph_nid_postfix(slot, &glyph) == FT_Get_Glyph(hostSlot, &native));
        Matches(glyph, native, fixture.library, false);
        FT_Done_Glyph_nid_postfix(glyph);
        FT_Done_Glyph(native);
    }
    slot->advance = {};
    hostSlot->advance = {};
    const auto savedOutline = slot->outline;
    const auto savedNativeOutline = hostSlot->outline;
    slot->outline = {1, 0, nullptr, nullptr, nullptr, 0};
    hostSlot->outline = {1, 0, nullptr, nullptr, nullptr, 0};
    glyph = &sentinel;
    native = &nativeSentinel;
    Require(FT_Get_Glyph_nid_postfix(slot, &glyph) == FT_Get_Glyph(hostSlot, &native));
    Require(glyph == nullptr && native == nullptr);
    slot->outline = {};
    hostSlot->outline = {};
    Require(FT_Get_Glyph_nid_postfix(slot, &glyph) == FT_Get_Glyph(hostSlot, &native));
    Matches(glyph, native, fixture.library, false);
    GuestGlyphRec* copy = &sentinel;
    FT_Glyph nativeCopy = &nativeSentinel;
    Require(FT_Glyph_Copy_nid_postfix(nullptr, &copy) == FT_Glyph_Copy(nullptr, &nativeCopy));
    Require(copy == &sentinel && nativeCopy == &nativeSentinel);
    Require(FT_Glyph_Copy_nid_postfix(glyph, nullptr) == FT_Glyph_Copy(native, nullptr));
    const auto clazz = glyph->clazz;
    const auto nativeClazz = native->clazz;
    glyph->clazz = nullptr;
    native->clazz = nullptr;
    Require(FT_Glyph_Copy_nid_postfix(glyph, &copy) == FT_Glyph_Copy(native, &nativeCopy));
    Require(copy == &sentinel && nativeCopy == &nativeSentinel);
    glyph->clazz = clazz;
    native->clazz = nativeClazz;
    Require(FT_Glyph_Copy_nid_postfix(glyph, &copy) == FT_Glyph_Copy(native, &nativeCopy));
    Matches(copy, nativeCopy, fixture.library, false);
    FT_Done_Glyph_nid_postfix(copy);
    FT_Done_Glyph(nativeCopy);
    auto& image = reinterpret_cast<GuestOutlineGlyphRec*>(glyph)->outline;
    auto& nativeImage = reinterpret_cast<FT_OutlineGlyph>(native)->outline;
    image.n_contours = nativeImage.n_contours = 1;
    copy = &sentinel;
    nativeCopy = &nativeSentinel;
    Require(FT_Glyph_Copy_nid_postfix(glyph, &copy) == FT_Glyph_Copy(native, &nativeCopy));
    Require(copy == nullptr && nativeCopy == nullptr);
    image.n_contours = nativeImage.n_contours = 0;
    glyph->advance = {std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max()};
    Require(FT_Glyph_Copy_nid_postfix(glyph, &copy) == 0);
    Require(copy->advance.x == glyph->advance.x && copy->advance.y == glyph->advance.y);
    FT_Done_Glyph_nid_postfix(copy);
    FT_Done_Glyph_nid_postfix(glyph);
    FT_Done_Glyph(native);
    slot->outline = savedOutline;
    hostSlot->outline = savedNativeOutline;
    FT_Done_Glyph_nid_postfix(nullptr);
    FT_Done_Glyph(nullptr);
}

static void OutlineCountLimits() {
    Fixture fixture;
    fixture.Load(1, FT_LOAD_NO_HINTING);
    const auto pointCount = std::min<std::size_t>(std::numeric_limits<decltype(FT_Outline{}.n_points)>::max(), std::numeric_limits<std::uint16_t>::max());
    std::vector<GuestVector> points(pointCount);
    std::vector<FT_Vector> nativePoints(pointCount);
    std::vector<unsigned char> tags(pointCount, 0xA5);
    std::vector<std::remove_pointer_t<decltype(FT_Outline{}.tags)>> nativeTags(pointCount, static_cast<std::remove_pointer_t<decltype(FT_Outline{}.tags)>>(0xA5));
    std::uint16_t contour = static_cast<std::uint16_t>(pointCount - 1);
    auto nativeContour = static_cast<std::remove_pointer_t<decltype(FT_Outline{}.contours)>>(contour);
    for (std::size_t index = 0; index < pointCount; ++index) {
        points[index] = {static_cast<std::int64_t>(index), -static_cast<std::int64_t>(index)};
        nativePoints[index] = {static_cast<FT_Pos>(index), -static_cast<FT_Pos>(index)};
    }
    auto* slot = fixture.face->glyph;
    const auto nativeSlot = fixture.host->glyph;
    const auto savedOutline = slot->outline;
    const auto savedNativeOutline = nativeSlot->outline;
    slot->outline = {1, static_cast<std::uint16_t>(pointCount), points.data(), tags.data(), &contour, 0};
    nativeSlot->outline = {1, static_cast<decltype(FT_Outline{}.n_points)>(pointCount), nativePoints.data(), nativeTags.data(), &nativeContour, 0};
    GuestGlyphRec* glyph = nullptr;
    FT_Glyph native = nullptr;
    Require(FT_Get_Glyph_nid_postfix(slot, &glyph) == FT_Get_Glyph(nativeSlot, &native));
    Matches(glyph, native, fixture.library, false);
    slot->outline = savedOutline;
    nativeSlot->outline = savedNativeOutline;
    reinterpret_cast<GuestOutlineGlyphRec*>(glyph)->outline.contours[0] = 0xFFFF;
    reinterpret_cast<FT_OutlineGlyph>(native)->outline.contours[0] = static_cast<decltype(nativeContour)>(0xFFFF);
    GuestGlyphRec* copy = nullptr;
    FT_Glyph nativeCopy = nullptr;
    Require(FT_Glyph_Copy_nid_postfix(glyph, &copy) == FT_Glyph_Copy(native, &nativeCopy));
    Matches(copy, nativeCopy, fixture.library, false);
    FT_Done_Glyph_nid_postfix(copy);
    FT_Done_Glyph(nativeCopy);
    if (pointCount < std::numeric_limits<std::uint16_t>::max()) {
        slot->outline = {0, static_cast<std::uint16_t>(pointCount + 1), nullptr, nullptr, nullptr, 0};
        Require(Throws([&] { FT_Get_Glyph_nid_postfix(slot, &copy); }, "FT_Get_Glyph"));
        slot->outline = {static_cast<std::uint16_t>(pointCount + 1), 0, nullptr, nullptr, nullptr, 0};
        Require(Throws([&] { FT_Get_Glyph_nid_postfix(slot, &copy); }, "FT_Get_Glyph"));
        slot->outline = savedOutline;
        auto& outline = reinterpret_cast<GuestOutlineGlyphRec*>(glyph)->outline;
        const auto saved = outline;
        outline.n_points = static_cast<std::uint16_t>(pointCount + 1);
        Require(Throws([&] { FT_Glyph_Copy_nid_postfix(glyph, &copy); }, "FT_Glyph_Copy"));
        outline = saved;
        outline.n_contours = static_cast<std::uint16_t>(pointCount + 1);
        Require(Throws([&] { FT_Glyph_Copy_nid_postfix(glyph, &copy); }, "FT_Glyph_Copy"));
        outline = saved;
    }
    FT_Done_Glyph_nid_postfix(glyph);
    FT_Done_Glyph(native);
}

static void UnsupportedChanges() {
    Fixture fixture;
    fixture.Load(1, FT_LOAD_NO_HINTING);
    GuestGlyphRec* glyph = nullptr;
    Require(FT_Get_Glyph_nid_postfix(fixture.face->glyph, &glyph) == 0);
    auto& outline = reinterpret_cast<GuestOutlineGlyphRec*>(glyph)->outline;
    const auto points = outline.points;
    std::array<GuestVector, 4> replacement{};
    outline.points = replacement.data();
    Require(Throws([&] { FT_Done_Glyph_nid_postfix(glyph); }, "FT_Done_Glyph"));
    outline.points = points;
    outline.flags ^= FT_OUTLINE_OWNER;
    Require(Throws([&] { FT_Done_Glyph_nid_postfix(glyph); }, "FT_Done_Glyph"));
    outline.flags ^= FT_OUTLINE_OWNER;
    const auto library = glyph->library;
    glyph->library = nullptr;
    GuestGlyphRec* copy = nullptr;
    Require(Throws([&] { FT_Glyph_Copy_nid_postfix(glyph, &copy); }, "FT_Glyph_Copy"));
    glyph->library = library;
    const auto clazz = glyph->clazz;
    const unsigned char foreignClass = 0;
    glyph->clazz = &foreignClass;
    Require(Throws([&] { FT_Glyph_Copy_nid_postfix(glyph, &copy); }, "FT_Glyph_Copy"));
    glyph->clazz = clazz;
    FT_Done_Glyph_nid_postfix(glyph);
    fixture.Load(1, FT_LOAD_RENDER);
    const auto slotBuffer = fixture.face->glyph->bitmap.buffer;
    fixture.face->glyph->bitmap.buffer = nullptr;
    Require(Throws([&] { FT_Get_Glyph_nid_postfix(fixture.face->glyph, &copy); }, "FT_Get_Glyph"));
    fixture.face->glyph->bitmap.buffer = slotBuffer;
    Require(FT_Get_Glyph_nid_postfix(fixture.face->glyph, &glyph) == 0);
    auto& bitmap = reinterpret_cast<GuestBitmapGlyphRec*>(glyph)->bitmap;
    const auto buffer = bitmap.buffer;
    bitmap.buffer = nullptr;
    Require(Throws([&] { FT_Done_Glyph_nid_postfix(glyph); }, "FT_Done_Glyph"));
    bitmap.buffer = buffer;
    FT_Done_Glyph_nid_postfix(glyph);
}

int main() {
    OutlineOwnership();
    BitmapOwnership();
    WideOutlineCoordinates();
    ErrorsAndEmptyGlyphs();
    OutlineCountLimits();
    UnsupportedChanges();
    std::puts("FreeType glyph ownership and reference comparisons passed");
}
