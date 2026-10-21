#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"

#include FT_GLYPH_H

#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <utility>

using namespace FreeType;

namespace {

template <typename TTarget, typename TSource> TTarget Count(TSource value, const char* function) {
    if (!std::in_range<TTarget>(value)) NotImplemented_nid_no_patch(function);
    return static_cast<TTarget>(value);
}

struct Glyph {
    GuestOutlineGlyphRec outline{};
    GuestBitmapGlyphRec bitmap{};
    GuestLibrary* library = nullptr;
    bool isBitmap = false;
    std::unique_ptr<FT_GlyphRec, decltype(&FT_Done_Glyph)> host{nullptr, &FT_Done_Glyph};
    std::vector<GuestVector> points;

    GuestGlyphRec* Root() { return isBitmap ? &bitmap.root : &outline.root; }

    void Publish(const GuestOutline& source, const char* function) {
        *Root() = {library, host->clazz, static_cast<std::uint32_t>(host->format), {host->advance.x, host->advance.y}};
        if (isBitmap) {
            const auto* native = reinterpret_cast<FT_BitmapGlyph>(host.get());
            bitmap.left = native->left;
            bitmap.top = native->top;
            const auto& image = native->bitmap;
            bitmap.bitmap = {image.rows, image.width, image.pitch, image.buffer, image.num_grays, image.pixel_mode, image.palette_mode, image.palette};
        } else {
            const auto& native = reinterpret_cast<FT_OutlineGlyph>(host.get())->outline;
            std::copy_n(source.points, points.size(), points.begin());
            outline.outline = {Count<std::uint16_t>(native.n_contours, function), Count<std::uint16_t>(native.n_points, function),
                               points.empty() ? nullptr : points.data(), reinterpret_cast<unsigned char*>(native.tags),
                               reinterpret_cast<std::uint16_t*>(native.contours), native.flags};
        }
    }
};

std::mutex glyphMutex;
std::unordered_map<GuestGlyphRec*, std::unique_ptr<Glyph>> glyphs;

FT_Vector Advance(const GuestVector& advance) {
    constexpr std::int64_t limit = 0x200000;
    return {static_cast<FT_Pos>(std::clamp(advance.x, -limit, limit)), static_cast<FT_Pos>(std::clamp(advance.y, -limit, limit))};
}

bool ValidAdvance(const GuestVector& advance) {
    return advance.x > -0x200000 && advance.x < 0x200000 && advance.y > -0x200000 && advance.y < 0x200000;
}

FT_Bitmap Bitmap(const GuestBitmap& bitmap) {
    return {bitmap.rows, bitmap.width, bitmap.pitch, bitmap.buffer, bitmap.num_grays, bitmap.pixel_mode, bitmap.palette_mode, bitmap.palette};
}

struct Outline {
    FT_Outline native{};
    std::vector<FT_Vector> points;

    Outline(const GuestOutline& outline, const char* function) {
        native = {Count<decltype(native.n_contours)>(outline.n_contours, function), Count<decltype(native.n_points)>(outline.n_points, function), nullptr,
                  reinterpret_cast<decltype(native.tags)>(outline.tags), reinterpret_cast<decltype(native.contours)>(outline.contours), outline.flags};
        if (outline.n_contours > outline.n_points) return;
        points.resize(outline.n_points);
        native.points = points.empty() ? nullptr : points.data();
    }
};

Glyph& Find(GuestGlyphRec* glyph, const char* function) {
    const auto found = glyphs.find(glyph);
    if (found == glyphs.end()) NotImplemented_nid_no_patch(function);
    auto& owner = *found->second;
    if (glyph->library != owner.library || glyph->clazz != owner.host->clazz) NotImplemented_nid_no_patch(function);
    return owner;
}

Glyph& Register(GuestLibrary* library, bool isBitmap, std::size_t pointCount) {
    auto owner = std::make_unique<Glyph>();
    owner->library = library;
    owner->isBitmap = isBitmap;
    owner->points.resize(pointCount);
    auto* address = owner->Root();
    return *glyphs.emplace(address, std::move(owner)).first->second;
}

}

extern "C" {

int APS5_VABI FT_Get_Glyph_nid_postfix(GuestGlyphSlotRec* slot, GuestGlyphRec** target) {
    if (!slot) return FT_Err_Invalid_Slot_Handle;
    if (!target) return FT_Err_Invalid_Argument;
    auto* library = static_cast<GuestLibrary*>(slot->library);
    FT_GlyphSlotRec native{};
    native.library = library ? library->host : nullptr;
    native.format = static_cast<FT_Glyph_Format>(slot->format);
    native.advance = Advance(slot->advance);
    if (!library || !ValidAdvance(slot->advance)) {
        FT_Glyph result = reinterpret_cast<FT_Glyph>(&native);
        const FT_Error error = FT_Get_Glyph(&native, &result);
        if (!result) *target = nullptr;
        return error;
    }
    if (native.format != FT_GLYPH_FORMAT_BITMAP && native.format != FT_GLYPH_FORMAT_OUTLINE) {
        FT_Glyph probe = nullptr;
        const FT_Error error = FT_New_Glyph(native.library, native.format, &probe);
        if (error) return error;
        FT_Done_Glyph(probe);
        NotImplemented_nid_no_patch(__func__);
    }
    Outline outline(native.format == FT_GLYPH_FORMAT_OUTLINE ? slot->outline : GuestOutline{}, __func__);
    native.outline = outline.native;
    native.bitmap = Bitmap(slot->bitmap);
    native.bitmap_left = slot->bitmap_left;
    native.bitmap_top = slot->bitmap_top;
    if (native.format == FT_GLYPH_FORMAT_BITMAP) {
        const auto* face = FaceOf(slot);
        if (!face || slot != &face->slot || slot->library != face->library) NotImplemented_nid_no_patch(__func__);
        const auto host = face->host->glyph;
        if (slot->bitmap.buffer != host->bitmap.buffer) NotImplemented_nid_no_patch(__func__);
        native.internal = host->internal;
    }
    std::lock_guard lock(glyphMutex);
    auto& owner = Register(library, native.format == FT_GLYPH_FORMAT_BITMAP, native.outline.n_points);
    auto* address = owner.Root();
    FT_Glyph result = reinterpret_cast<FT_Glyph>(&native);
    const FT_Error error = FT_Get_Glyph(&native, &result);
    if (error) {
        glyphs.erase(address);
        if (!result) *target = nullptr;
        return error;
    }
    owner.host.reset(result);
    owner.Publish(slot->outline, __func__);
    *target = address;
    return 0;
}

int APS5_VABI FT_Glyph_Copy_nid_postfix(GuestGlyphRec* source, GuestGlyphRec** target) {
    if (!target || !source || !source->clazz) return FT_Err_Invalid_Argument;
    std::lock_guard lock(glyphMutex);
    auto& original = Find(source, __func__);
    *target = nullptr;
    if (!source->clazz) return FT_Err_Invalid_Argument;
    if (source->library != original.library || source->clazz != original.host->clazz) NotImplemented_nid_no_patch(__func__);
    const GuestVector advance = source->advance;
    FT_OutlineGlyphRec nativeOutline{};
    FT_BitmapGlyphRec nativeBitmap{};
    FT_Glyph native = original.isBitmap ? &nativeBitmap.root : &nativeOutline.root;
    *native = *original.host;
    native->format = static_cast<FT_Glyph_Format>(source->format);
    Outline outline(original.isBitmap ? GuestOutline{} : original.outline.outline, __func__);
    nativeOutline.outline = outline.native;
    nativeBitmap.bitmap = Bitmap(original.bitmap.bitmap);
    nativeBitmap.left = original.bitmap.left;
    nativeBitmap.top = original.bitmap.top;
    auto& copy = Register(original.library, original.isBitmap, outline.native.n_points);
    auto* address = copy.Root();
    FT_Glyph result = nullptr;
    const FT_Error error = FT_Glyph_Copy(native, &result);
    if (error) {
        glyphs.erase(address);
        return error;
    }
    copy.host.reset(result);
    copy.Publish(original.outline.outline, __func__);
    address->advance = advance;
    *target = address;
    return 0;
}

void APS5_VABI FT_Done_Glyph_nid_postfix(GuestGlyphRec* glyph) {
    if (!glyph) return;
    std::lock_guard lock(glyphMutex);
    auto& owner = Find(glyph, __func__);
    if (owner.isBitmap) {
        if (owner.bitmap.bitmap.buffer != reinterpret_cast<FT_BitmapGlyph>(owner.host.get())->bitmap.buffer) NotImplemented_nid_no_patch(__func__);
    } else {
        const auto& outline = owner.outline.outline;
        const auto& native = reinterpret_cast<FT_OutlineGlyph>(owner.host.get())->outline;
        if (outline.points != (owner.points.empty() ? nullptr : owner.points.data()) ||
            static_cast<const void*>(outline.tags) != static_cast<const void*>(native.tags) ||
            static_cast<const void*>(outline.contours) != static_cast<const void*>(native.contours) ||
            (outline.flags & FT_OUTLINE_OWNER) != (native.flags & FT_OUTLINE_OWNER)) NotImplemented_nid_no_patch(__func__);
    }
    glyphs.erase(glyph);
}

}
