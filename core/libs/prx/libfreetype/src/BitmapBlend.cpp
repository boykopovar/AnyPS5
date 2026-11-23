#include "prx/libfreetype/include/BitmapBlend.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"

#include FT_BITMAP_H

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

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

bool CopyPixels(const unsigned char* buffer, std::size_t bytes, std::vector<unsigned char>& pixels) {
    constexpr std::size_t Chunk = 0x1000;
    for (std::size_t offset = 0; offset < bytes;) {
        const std::size_t size = std::min(Chunk, bytes - offset);
        pixels.resize(offset + size);
        if (!GuestCopyIn_nid_no_patch(pixels.data() + offset, buffer + offset, size)) return false;
        offset += size;
    }
    return true;
}

}

extern "C" {

int APS5_VABI freetypeLegacyBitmapBlend_nid_no_patch(FreeType::GuestLibrary* library, const FreeType::GuestBitmap* source, FreeType::GuestVector sourceOffset,
                                                     FreeType::GuestBitmap* target, FreeType::GuestVector* targetOffset, LegacyFreeType::Color color) {
    const FT_Color nativeColor{color.blue, color.green, color.red, color.alpha};
    const FT_Library host = library ? library->host : nullptr;
    if (!host || !source || !target || !targetOffset) return FT_Bitmap_Blend(nullptr, nullptr, {}, nullptr, nullptr, nativeColor);
    FreeType::GuestBitmap sourceRecord{};
    FreeType::GuestBitmap targetRecord{};
    FreeType::GuestVector offset{};
    if (!GuestCopyIn_nid_no_patch(&sourceRecord, source, sizeof(sourceRecord)) || !GuestCopyIn_nid_no_patch(&targetRecord, target, sizeof(targetRecord)) ||
        !GuestCopyIn_nid_no_patch(&offset, targetOffset, sizeof(offset))) {
        return FT_Err_Invalid_Argument;
    }
    const FT_Vector nativeSourceOffset = Vector(sourceOffset, __func__);
    FT_Vector nativeTargetOffset = Vector(offset, __func__);
    FT_Bitmap nativeSource = Native(sourceRecord);
    FT_Bitmap nativeTarget = Native(targetRecord);
    std::vector<unsigned char> sourcePixels;
    if (sourceRecord.pixel_mode != FT_PIXEL_MODE_NONE && sourceRecord.width && sourceRecord.rows) {
        if (!CopyPixels(sourceRecord.buffer, PixelBytes(sourceRecord), sourcePixels)) return FT_Err_Invalid_Argument;
        if (!sourcePixels.empty()) nativeSource.buffer = sourcePixels.data();
    }
    const std::size_t targetPixels = PixelBytes(targetRecord);
    GuestArena::HostWrite targetWrite(target, sizeof(FreeType::GuestBitmap));
    GuestArena::HostWrite offsetWrite(targetOffset, sizeof(FreeType::GuestVector));
    GuestArena::HostWrite pixelWrite(targetRecord.buffer, targetPixels);
    if (!targetWrite.Open() || !offsetWrite.Open() || (targetRecord.buffer && !pixelWrite.Open()) ||
        !GuestWritable_nid_no_patch(target, sizeof(FreeType::GuestBitmap)) || !GuestWritable_nid_no_patch(targetOffset, sizeof(FreeType::GuestVector)) ||
        !GuestWritable_nid_no_patch(targetRecord.buffer, targetPixels)) {
        return FT_Err_Invalid_Argument;
    }
    const FT_Error error = FT_Bitmap_Blend(host, &nativeSource, nativeSourceOffset, &nativeTarget, &nativeTargetOffset, nativeColor);
    for (const auto& face : library->faces) {
        if (target == &face->slot.bitmap && face->host != nullptr && face->host->glyph != nullptr) face->host->glyph->bitmap = nativeTarget;
    }
    const FreeType::GuestVector resultOffset{nativeTargetOffset.x, nativeTargetOffset.y};
    if (!FreeTypeSyncBitmap_nid_no_patch(target, nativeTarget) || !GuestCopyOut_nid_no_patch(targetOffset, &resultOffset, sizeof(resultOffset))) return FT_Err_Invalid_Argument;
    return error;
}
APS5_EXPORT("C3-qsnriefE", freetypeLegacyBitmapBlend_nid_no_patch);

}
