// Derived from FreeType src/base/ftbitmap.c (FreeType 2.4.12, commit cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9).
// Copyright 2004-2009, 2011, 2013 by David Turner, Robert Wilhelm, and Werner Lemberg.
// This file is part of the FreeType project, and may only be used, modified, and distributed
// under the terms of the FreeType project license, LICENSE.TXT. By continuing to use, modify,
// or distribute this file you indicate that you have read the license and understand and accept it fully.

#include "prx/libSceFreeTypeOt/include/Bitmap.hpp"
#include "prx/libSceFreeTypeOt/include/GlyphSlot.hpp"
#include "prx/libSceFreeTypeOt/include/Memory.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdint>
#include <cstring>
#include <limits>

using namespace FreeType;

namespace {

constexpr std::int64_t IntMax = std::numeric_limits<std::int32_t>::max();
constexpr std::int64_t IntMin = std::numeric_limits<std::int32_t>::min();

std::int32_t CopyBitmap(GuestBitmapLibrary* library, const GuestBitmap* source, GuestBitmap* target, const char* function) {
    if (!library) NotImplemented_nid_no_patch(function);
    GuestMemoryRec* memory = library->memory;
    if (!source->buffer) {
        *target = *source;
        return 0;
    }
    std::int64_t pitch = source->pitch;
    if (pitch == IntMin) NotImplemented_nid_no_patch(function);
    if (pitch < 0) pitch = -pitch;
    const std::int64_t size = pitch * static_cast<std::int32_t>(source->rows);
    if (size > IntMax || size < IntMin) NotImplemented_nid_no_patch(function);
    if (size > 0 && !memory) NotImplemented_nid_no_patch(function);
    std::int32_t error = 0;
    auto* buffer = static_cast<unsigned char*>(ft_mem_qalloc_nid_postfix(memory, size, &error));
    if (!error) {
        *target = *source;
        target->buffer = buffer;
        if (size > 0) std::memcpy(buffer, source->buffer, static_cast<std::size_t>(size));
    }
    return error;
}

}

extern "C" {

int APS5_VABI sceFreeTypeOtGlyphSlotOwnBitmap_nid_no_patch(GuestGlyphSlotRec* slot) {
    const char* function = __func__;
    if (!slot || slot->format != GlyphFormatBitmap) return 0;
    auto* internal = static_cast<GuestSlotInternalRec*>(slot->internal);
    if (!internal) NotImplemented_nid_no_patch(function);
    if (internal->flags & GlyphOwnBitmap) return 0;
    GuestBitmap bitmap{};
    const std::int32_t error = CopyBitmap(static_cast<GuestBitmapLibrary*>(slot->library), &slot->bitmap, &bitmap, function);
    if (error) return error;
    slot->bitmap = bitmap;
    internal->flags |= GlyphOwnBitmap;
    return 0;
}
APS5_EXPORT("nSIux4OQyJM", sceFreeTypeOtGlyphSlotOwnBitmap_nid_no_patch);

}
