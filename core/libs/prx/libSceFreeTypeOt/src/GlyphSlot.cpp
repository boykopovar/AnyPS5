// Derived from FreeType src/base/ftobjs.c (FreeType 2.4.12, commit cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9).
// Copyright 1996-2013 by David Turner, Robert Wilhelm, and Werner Lemberg.
// This file is part of the FreeType project, and may only be used, modified, and distributed
// under the terms of the FreeType project license, LICENSE.TXT. By continuing to use, modify,
// or distribute this file you indicate that you have read the license and understand and accept it fully.

#include "prx/libSceFreeTypeOt/include/GlyphSlot.hpp"
#include "prx/libSceFreeTypeOt/include/Memory.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdint>

using namespace FreeType;

namespace {

GuestSlotInternalRec* Internal(GuestGlyphSlotRec* slot) {
    return static_cast<GuestSlotInternalRec*>(slot->internal);
}

GuestMemoryRec* FaceMemory(GuestGlyphSlotRec* slot, const char* function) {
    if (!slot->face) NotImplemented_nid_no_patch(function);
    return static_cast<GuestMemoryRec*>(static_cast<GuestFaceRec*>(slot->face)->memory);
}

void FreeOwned(GuestMemoryRec* memory, GuestGlyphSlotRec* slot, const char* function) {
    if (slot->bitmap.buffer && !memory) NotImplemented_nid_no_patch(function);
    ft_mem_free_nid_postfix(memory, slot->bitmap.buffer);
    slot->bitmap.buffer = nullptr;
}

void FreeBitmap(GuestGlyphSlotRec* slot, const char* function) {
    if (!slot) NotImplemented_nid_no_patch(function);
    GuestSlotInternalRec* internal = Internal(slot);
    if (internal && (internal->flags & GlyphOwnBitmap)) {
        FreeOwned(FaceMemory(slot, function), slot, function);
        internal->flags &= ~GlyphOwnBitmap;
    } else {
        slot->bitmap.buffer = nullptr;
    }
}

}

extern "C" {

void APS5_VABI ft_glyphslot_free_bitmap_nid_postfix(GuestGlyphSlotRec* slot) {
    FreeBitmap(slot, __func__);
}

void APS5_VABI ft_glyphslot_set_bitmap_nid_postfix(GuestGlyphSlotRec* slot, std::uint8_t* buffer) {
    FreeBitmap(slot, __func__);
    slot->bitmap.buffer = buffer;
}

int APS5_VABI ft_glyphslot_alloc_bitmap_nid_postfix(GuestGlyphSlotRec* slot, std::uint64_t size) {
    const char* function = __func__;
    if (!slot || !slot->internal) NotImplemented_nid_no_patch(function);
    GuestMemoryRec* memory = FaceMemory(slot, function);
    GuestSlotInternalRec* internal = Internal(slot);
    if (internal->flags & GlyphOwnBitmap) {
        FreeOwned(memory, slot, function);
    } else {
        internal->flags |= GlyphOwnBitmap;
    }
    const auto bytes = static_cast<std::int64_t>(size);
    if (bytes > 0 && !memory) NotImplemented_nid_no_patch(function);
    std::int32_t error = 0;
    slot->bitmap.buffer = static_cast<unsigned char*>(ft_mem_alloc_nid_postfix(memory, bytes, &error));
    return error;
}

}
