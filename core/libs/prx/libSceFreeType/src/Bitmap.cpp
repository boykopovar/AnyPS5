#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_BITMAP_H

#include <cerrno>
#include <cstdint>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
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

GuestBitmap GuestRecord(const FT_Bitmap& bitmap) {
    return {bitmap.rows, bitmap.width, bitmap.pitch, bitmap.buffer, bitmap.num_grays, bitmap.pixel_mode, bitmap.palette_mode, bitmap.palette};
}

void SyncOwner(const GuestBitmap* guest, const FT_Bitmap& bitmap) {
    auto& owners = Owners();
    std::lock_guard lock(owners.mutex);
    const auto found = owners.bitmaps.find(guest);
    if (found != owners.bitmaps.end() && found->second) *found->second = bitmap;
}

FT_Library ReadGuestLibrary(GuestLibrary* library, const char* function) {
    FT_Library host = nullptr;
    const int saved = errno;
    const bool read = GuestCopyIn_nid_no_patch(&host, &library->host, sizeof(host));
    errno = saved;
    if (!read) throw std::invalid_argument(std::string(function) + ": unreadable library handle");
    return host;
}

GuestBitmap ReadGuestBitmap(const GuestBitmap* bitmap, const char* function) {
    GuestBitmap record{};
    const int saved = errno;
    const bool read = GuestCopyIn_nid_no_patch(&record, bitmap, sizeof(record));
    errno = saved;
    if (!read) throw std::invalid_argument(std::string(function) + ": unreadable bitmap");
    return record;
}

void RequireWritableBitmap(GuestBitmap* bitmap, const char* function) {
    const int saved = errno;
    const bool writable = GuestWritable_nid_no_patch(bitmap, sizeof(*bitmap));
    errno = saved;
    if (!writable) throw std::invalid_argument(std::string(function) + ": unwritable bitmap");
}

void WriteGuestBitmap(GuestBitmap* guest, const FT_Bitmap& bitmap, const char* function) {
    const int saved = errno;
    const bool written = FreeTypeSyncBitmap_nid_no_patch(guest, bitmap);
    errno = saved;
    if (!written) throw std::invalid_argument(std::string(function) + ": unwritable bitmap");
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

bool FreeTypeSyncBitmap_nid_no_patch(GuestBitmap* guest, const FT_Bitmap& bitmap) {
    const GuestBitmap record = GuestRecord(bitmap);
    if (!GuestCopyOut_nid_no_patch(guest, &record, sizeof(record))) return false;
    SyncOwner(guest, bitmap);
    return true;
}

void APS5_VABI FT_Bitmap_New_nid_postfix(GuestBitmap* bitmap) {
    if (bitmap) WriteGuestBitmap(bitmap, {}, __func__);
}

void APS5_VABI FT_Bitmap_Init_nid_postfix(GuestBitmap* bitmap) {
    FT_Bitmap_New_nid_postfix(bitmap);
}

int APS5_VABI FT_Bitmap_Copy_nid_postfix(GuestLibrary* library, const GuestBitmap* source, GuestBitmap* target) {
    if (!library) return FT_Err_Invalid_Library_Handle;
    if (!source || !target) return FT_Err_Invalid_Argument;
    const FT_Library host = ReadGuestLibrary(library, __func__);
    const GuestBitmap sourceRecord = ReadGuestBitmap(source, __func__);
    if (source == target) {
        WriteGuestBitmap(target, NativeBitmap(sourceRecord), __func__);
        return 0;
    }
    const GuestBitmap targetRecord = ReadGuestBitmap(target, __func__);
    if (sourceRecord.buffer && sourceRecord.buffer == targetRecord.buffer) {
        throw std::runtime_error("FT_Bitmap_Copy: overlapping source and target buffers are not implemented");
    }
    RequireWritableBitmap(target, __func__);
    const FT_Bitmap nativeSource = NativeBitmap(sourceRecord);
    FT_Bitmap nativeTarget = NativeBitmap(targetRecord);
    const FT_Error error = FT_Bitmap_Copy(host, &nativeSource, &nativeTarget);
    WriteGuestBitmap(target, nativeTarget, __func__);
    return error;
}

int APS5_VABI FT_Bitmap_Convert_nid_postfix(GuestLibrary* library, const GuestBitmap* source, GuestBitmap* target, std::int32_t alignment) {
    if (!library) return FT_Err_Invalid_Library_Handle;
    if (!source || !target) return FT_Err_Invalid_Argument;
    const FT_Library host = ReadGuestLibrary(library, __func__);
    const GuestBitmap sourceRecord = ReadGuestBitmap(source, __func__);
    const GuestBitmap targetRecord = source == target ? sourceRecord : ReadGuestBitmap(target, __func__);
    if (source == target || (sourceRecord.buffer && sourceRecord.buffer == targetRecord.buffer)) {
        throw std::runtime_error("FT_Bitmap_Convert: overlapping source and target buffers are not implemented");
    }
    RequireWritableBitmap(target, __func__);
    const FT_Bitmap nativeSource = NativeBitmap(sourceRecord);
    FT_Bitmap nativeTarget = NativeBitmap(targetRecord);
    const FT_Error error = FT_Bitmap_Convert(host, &nativeSource, &nativeTarget, alignment);
    WriteGuestBitmap(target, nativeTarget, __func__);
    return error;
}

int APS5_VABI FT_Bitmap_Embolden_nid_postfix(GuestLibrary* library, GuestBitmap* bitmap, std::int64_t xStrength, std::int64_t yStrength) {
    if (!library) return FT_Err_Invalid_Library_Handle;
    if (!bitmap) return FT_Err_Invalid_Argument;
    for (const std::int64_t strength : {xStrength, yStrength}) {
        if (strength < std::numeric_limits<FT_Pos>::min() || strength > std::numeric_limits<FT_Pos>::max()) {
            throw std::runtime_error("FT_Bitmap_Embolden: a strength outside the host FT_Pos range is not implemented");
        }
        if (strength > std::numeric_limits<FT_Pos>::max() - 32) return FT_Err_Invalid_Argument;
        constexpr std::int64_t minimumRounded = static_cast<std::int64_t>(std::numeric_limits<FT_Int>::min()) * 64 - 32;
        constexpr std::int64_t maximumRounded = static_cast<std::int64_t>(std::numeric_limits<FT_Int>::max()) * 64 + 31;
        if (strength < minimumRounded || strength > maximumRounded) return FT_Err_Invalid_Argument;
    }
    const FT_Library host = ReadGuestLibrary(library, __func__);
    FT_Bitmap native = NativeBitmap(ReadGuestBitmap(bitmap, __func__));
    RequireWritableBitmap(bitmap, __func__);
    const FT_Error error = FT_Bitmap_Embolden(host, &native, static_cast<FT_Pos>(xStrength), static_cast<FT_Pos>(yStrength));
    WriteGuestBitmap(bitmap, native, __func__);
    return error;
}

int APS5_VABI FT_GlyphSlot_Own_Bitmap_nid_postfix(GuestGlyphSlotRec* slot) {
    GuestFace* owner = FaceOf(slot);
    const FT_Error error = FT_GlyphSlot_Own_Bitmap(owner ? owner->host->glyph : nullptr);
    if (owner) SyncSlot(*owner);
    return error;
}

int APS5_VABI FT_Bitmap_Done_nid_postfix(GuestLibrary* library, GuestBitmap* bitmap) {
    if (!library) return FT_Err_Invalid_Library_Handle;
    if (!bitmap) return FT_Err_Invalid_Argument;
    const FT_Library host = ReadGuestLibrary(library, __func__);
    FT_Bitmap native = NativeBitmap(ReadGuestBitmap(bitmap, __func__));
    RequireWritableBitmap(bitmap, __func__);
    const FT_Error error = FT_Bitmap_Done(host, &native);
    WriteGuestBitmap(bitmap, native, __func__);
    return error;
}

}
