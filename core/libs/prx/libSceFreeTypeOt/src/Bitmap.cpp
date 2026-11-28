// Derived from FreeType src/base/ftbitmap.c (FreeType 2.4.12, commit cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9).
// Copyright 2004-2009, 2011, 2013 by David Turner, Robert Wilhelm, and Werner Lemberg.
// This file is part of the FreeType project, and may only be used, modified, and distributed
// under the terms of the FreeType project license, LICENSE.TXT. By continuing to use, modify,
// or distribute this file you indicate that you have read the license and understand and accept it fully.

#include "prx/libSceFreeTypeOt/include/Bitmap.hpp"
#include "prx/libSceFreeTypeOt/include/GlyphSlot.hpp"
#include "prx/libSceFreeTypeOt/include/Memory.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>

using namespace FreeType;

namespace {

constexpr std::int64_t IntMax = std::numeric_limits<std::int32_t>::max();
constexpr std::int64_t IntMin = std::numeric_limits<std::int32_t>::min();

bool GuestCopyBytesInPreservingErrno(void* destination, const void* source, std::size_t size) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, size);
    errno = savedErrno;
    return ok;
}

bool GuestCopyBytesOutPreservingErrno(void* destination, const void* source, std::size_t size) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, source, size);
    errno = savedErrno;
    return ok;
}

std::int32_t CopyBitmap(GuestMemoryRec* memory, const GuestBitmap& source, GuestBitmap& target, const char* function, unsigned char*& allocatedBuffer) {
    allocatedBuffer = nullptr;
    if (!source.buffer) {
        target = source;
        return 0;
    }
    std::int64_t pitch = source.pitch;
    if (pitch == IntMin) NotImplemented_nid_no_patch(function);
    if (pitch < 0) pitch = -pitch;
    const std::int64_t size = pitch * static_cast<std::int32_t>(source.rows);
    if (size > IntMax || size < IntMin) NotImplemented_nid_no_patch(function);
    if (size > 0 && !memory) NotImplemented_nid_no_patch(function);
    std::int32_t error = 0;
    auto* buffer = static_cast<unsigned char*>(OtMemQAlloc(memory, size, &error));
    if (!error) {
        if (size > 0 && !GuestCopyBytesInPreservingErrno(buffer, source.buffer, static_cast<std::size_t>(size))) {
            OtMemFree(memory, buffer);
            NotImplemented_nid_no_patch(function);
        }
        target = source;
        target.buffer = buffer;
        allocatedBuffer = buffer;
    }
    return error;
}

}

extern "C" {

int APS5_VABI sceFreeTypeOtGlyphSlotOwnBitmap_nid_no_patch(GuestGlyphSlotRec* slot) {
    const char* function = __func__;
    if (!slot) return 0;
    GuestGlyphSlotRec guestSlot{};
    if (!GuestCopyBytesInPreservingErrno(&guestSlot, slot, sizeof(guestSlot))) NotImplemented_nid_no_patch(function);
    if (guestSlot.format != GlyphFormatBitmap) return 0;
    auto* internal = static_cast<GuestSlotInternalRec*>(guestSlot.internal);
    if (!internal) NotImplemented_nid_no_patch(function);
    GuestSlotInternalRec guestInternal{};
    if (!GuestCopyBytesInPreservingErrno(&guestInternal, internal, sizeof(guestInternal))) NotImplemented_nid_no_patch(function);
    if (guestInternal.flags & GlyphOwnBitmap) return 0;
    auto* library = static_cast<GuestBitmapLibrary*>(guestSlot.library);
    if (!library) NotImplemented_nid_no_patch(function);
    GuestBitmapLibrary guestLibrary{};
    if (!GuestCopyBytesInPreservingErrno(&guestLibrary, library, sizeof(guestLibrary))) NotImplemented_nid_no_patch(function);
    GuestBitmap bitmap{};
    unsigned char* allocatedBuffer = nullptr;
    const std::int32_t error = CopyBitmap(guestLibrary.memory, guestSlot.bitmap, bitmap, function, allocatedBuffer);
    if (error) return error;
    const std::uint32_t updatedFlags = guestInternal.flags | GlyphOwnBitmap;
    int savedErrno = errno;
    const bool updatedSlot = GuestCopyOut_nid_no_patch(&slot->bitmap, &bitmap, sizeof(bitmap));
    errno = savedErrno;
    if (!updatedSlot) {
        if (allocatedBuffer) OtMemFree(guestLibrary.memory, allocatedBuffer);
        NotImplemented_nid_no_patch(function);
    }
    if (!GuestCopyBytesOutPreservingErrno(&internal->flags, &updatedFlags, sizeof(updatedFlags))) {
        savedErrno = errno;
        GuestCopyOut_nid_no_patch(&slot->bitmap, &guestSlot.bitmap, sizeof(guestSlot.bitmap));
        errno = savedErrno;
        if (allocatedBuffer) OtMemFree(guestLibrary.memory, allocatedBuffer);
        NotImplemented_nid_no_patch(function);
    }
    return 0;
}
APS5_EXPORT("nSIux4OQyJM", sceFreeTypeOtGlyphSlotOwnBitmap_nid_no_patch);

}
