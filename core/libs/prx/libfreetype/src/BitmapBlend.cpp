#include "prx/libfreetype/include/BitmapBlend.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"

#include FT_BITMAP_H

#include <cstddef>
#include <utility>

static_assert(sizeof(FT_Bitmap) == sizeof(FreeType::GuestBitmap) && offsetof(FT_Bitmap, buffer) == offsetof(FreeType::GuestBitmap, buffer) &&
              offsetof(FT_Bitmap, palette) == offsetof(FreeType::GuestBitmap, palette));

namespace {

FT_Vector Vector(const FreeType::GuestVector& vector, const char* function) {
    if (!std::in_range<FT_Pos>(vector.x) || !std::in_range<FT_Pos>(vector.y)) NotImplemented_nid_no_patch(function);
    return {static_cast<FT_Pos>(vector.x), static_cast<FT_Pos>(vector.y)};
}

FT_Bitmap Native(const FreeType::GuestBitmap& bitmap) {
    return {bitmap.rows, bitmap.width, bitmap.pitch, bitmap.buffer, bitmap.num_grays, bitmap.pixel_mode, bitmap.palette_mode, bitmap.palette};
}

std::size_t PixelBytes(const FreeType::GuestBitmap& bitmap) {
    const auto pitch = static_cast<std::size_t>(bitmap.pitch < 0 ? -static_cast<std::int64_t>(bitmap.pitch) : bitmap.pitch);
    return bitmap.buffer ? pitch * bitmap.rows : 0;
}

}

extern "C" {

int APS5_VABI freetypeLegacyBitmapBlend_nid_no_patch(FreeType::GuestLibrary* library, const FreeType::GuestBitmap* source, FreeType::GuestVector sourceOffset,
                                                     FreeType::GuestBitmap* target, FreeType::GuestVector* targetOffset, LegacyFreeType::Color color) {
    const FT_Color nativeColor{color.blue, color.green, color.red, color.alpha};
    const FT_Library host = library ? library->host : nullptr;
    if (!host || !source || !target || !targetOffset) return FT_Bitmap_Blend(nullptr, nullptr, {}, nullptr, nullptr, nativeColor);
    for (const auto& face : library->faces) {
        if (target == &face->slot.bitmap) NotImplemented_nid_no_patch(__func__);
    }
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(source, sizeof(*source));
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(target, sizeof(*target));
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(targetOffset, sizeof(*targetOffset));
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(source->buffer, PixelBytes(*source));
    const FT_Vector nativeSourceOffset = Vector(sourceOffset, __func__);
    FT_Vector nativeTargetOffset = Vector(*targetOffset, __func__);
    const FT_Bitmap nativeSource = Native(*source);
    FT_Bitmap nativeTarget = Native(*target);
    GuestArena::HostWrite targetWrite(target, sizeof(*target));
    GuestArena::HostWrite offsetWrite(targetOffset, sizeof(*targetOffset));
    GuestArena::HostWrite pixelWrite(target->buffer, PixelBytes(*target));
    if (!targetWrite.Open() || !offsetWrite.Open() || (target->buffer && !pixelWrite.Open())) return FT_Err_Invalid_Argument;
    const FT_Error error = FT_Bitmap_Blend(host, &nativeSource, nativeSourceOffset, &nativeTarget, &nativeTargetOffset, nativeColor);
    *target = {nativeTarget.rows, nativeTarget.width, nativeTarget.pitch, nativeTarget.buffer, nativeTarget.num_grays, nativeTarget.pixel_mode,
               nativeTarget.palette_mode, nativeTarget.palette};
    *targetOffset = {nativeTargetOffset.x, nativeTargetOffset.y};
    return error;
}
APS5_EXPORT("C3-qsnriefE", freetypeLegacyBitmapBlend_nid_no_patch);

}
