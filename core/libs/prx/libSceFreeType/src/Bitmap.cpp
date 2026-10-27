#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_BITMAP_H

#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

using namespace FreeType;

namespace {

struct BitmapOwners {
    std::mutex mutex;
    std::unordered_map<const GuestBitmap*, FT_Bitmap*> bitmaps;
};

BitmapOwners& Owners() {
    static BitmapOwners owners;
    return owners;
}

FT_Bitmap NativeBitmap(const GuestBitmap& bitmap) {
    return {bitmap.rows, bitmap.width, bitmap.pitch, bitmap.buffer, bitmap.num_grays, bitmap.pixel_mode, bitmap.palette_mode, bitmap.palette};
}

void SyncBitmap(GuestBitmap& guest, const FT_Bitmap& bitmap) {
    guest = {bitmap.rows, bitmap.width, bitmap.pitch, bitmap.buffer, bitmap.num_grays, bitmap.pixel_mode, bitmap.palette_mode, bitmap.palette};
    auto& owners = Owners();
    std::lock_guard lock(owners.mutex);
    const auto found = owners.bitmaps.find(&guest);
    if (found != owners.bitmaps.end() && found->second) *found->second = bitmap;
}

}

namespace FreeType {

void RegisterBitmap_nid_no_patch(GuestFace& face) {
    auto& owners = Owners();
    std::lock_guard lock(owners.mutex);
    owners.bitmaps.insert_or_assign(&face.slot.bitmap, face.host->glyph ? &face.host->glyph->bitmap : nullptr);
}

void UnregisterBitmap_nid_no_patch(const GuestBitmap* bitmap) {
    auto& owners = Owners();
    std::lock_guard lock(owners.mutex);
    owners.bitmaps.erase(bitmap);
}

}

extern "C" {

void APS5_VABI FT_Bitmap_New_nid_postfix(GuestBitmap* bitmap) {
    if (bitmap) SyncBitmap(*bitmap, {});
}

int APS5_VABI FT_Bitmap_Copy_nid_postfix(GuestLibrary* library, const GuestBitmap* source, GuestBitmap* target) {
    if (!library) return FT_Err_Invalid_Library_Handle;
    if (!source || !target) return FT_Err_Invalid_Argument;
    if (source == target) {
        SyncBitmap(*target, NativeBitmap(*target));
        return 0;
    }
    if (source->buffer && source->buffer == target->buffer) {
        throw std::runtime_error("FT_Bitmap_Copy: overlapping source and target buffers are not implemented");
    }
    const FT_Bitmap nativeSource = NativeBitmap(*source);
    FT_Bitmap nativeTarget = NativeBitmap(*target);
    const FT_Error error = FT_Bitmap_Copy(library->host, &nativeSource, &nativeTarget);
    SyncBitmap(*target, nativeTarget);
    return error;
}

int APS5_VABI FT_Bitmap_Convert_nid_postfix(GuestLibrary* library, const GuestBitmap* source, GuestBitmap* target, std::int32_t alignment) {
    if (!library) return FT_Err_Invalid_Library_Handle;
    if (!source || !target) return FT_Err_Invalid_Argument;
    if (source == target || (source->buffer && source->buffer == target->buffer)) {
        throw std::runtime_error("FT_Bitmap_Convert: overlapping source and target buffers are not implemented");
    }
    const FT_Bitmap nativeSource = NativeBitmap(*source);
    FT_Bitmap nativeTarget = NativeBitmap(*target);
    const FT_Error error = FT_Bitmap_Convert(library->host, &nativeSource, &nativeTarget, alignment);
    SyncBitmap(*target, nativeTarget);
    return error;
}

int APS5_VABI FT_Bitmap_Done_nid_postfix(GuestLibrary* library, GuestBitmap* bitmap) {
    if (!library) return FT_Err_Invalid_Library_Handle;
    if (!bitmap) return FT_Err_Invalid_Argument;
    FT_Bitmap native = NativeBitmap(*bitmap);
    const FT_Error error = FT_Bitmap_Done(library->host, &native);
    SyncBitmap(*bitmap, native);
    return error;
}

}
