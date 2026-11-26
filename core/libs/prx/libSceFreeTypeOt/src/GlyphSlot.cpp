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

constexpr int InvalidArgument = 0x06;

GuestSlotInternalRec* Internal(GuestGlyphSlotRec* slot) {
    return static_cast<GuestSlotInternalRec*>(slot->internal);
}

GuestMemoryRec* FaceMemory(GuestGlyphSlotRec* slot, const char* function) {
    if (!slot->face) NotImplemented_nid_no_patch(function);
    return static_cast<GuestMemoryRec*>(static_cast<GuestFaceRec*>(slot->face)->memory);
}

void FreeOwned(GuestMemoryRec* memory, GuestGlyphSlotRec* slot, const char* function) {
    if (slot->bitmap.buffer && !memory) NotImplemented_nid_no_patch(function);
    OtMemFree(memory, slot->bitmap.buffer);
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

GuestDriverRec* DriverOf(GuestGlyphSlotRec* slot, const char* function) {
    auto* face = static_cast<GuestFaceRec*>(slot->face);
    if (!face || !face->driver) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    auto* driver = static_cast<GuestDriverRec*>(face->driver);
    if (!driver->clazz) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return driver;
}

bool UsesOutlines(const GuestDriverRec* driver, const char* function) {
    if (!driver->root_clazz) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return !(driver->root_clazz->module_flags & ModuleDriverNoOutlines);
}

void* Allocate(GuestMemoryRec* memory, std::int64_t size, std::int32_t* error, const char* function) {
    if (size > 0 && !memory) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return ft_mem_alloc_nid_postfix(memory, size, error);
}

void Free(GuestMemoryRec* memory, const void* block, const char* function) {
    if (block && !memory) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    ft_mem_free_nid_postfix(memory, block);
}

void LoaderDone(GuestGlyphLoaderRec* loader, const char* function) {
    if (!loader) return;
    GuestMemoryRec* memory = loader->memory;
    Free(memory, loader->base.outline.points, function);
    Free(memory, loader->base.outline.tags, function);
    Free(memory, loader->base.outline.contours, function);
    Free(memory, loader->base.extra_points, function);
    Free(memory, loader->base.subglyphs, function);
    Free(memory, loader, function);
}

std::int32_t SlotInit(GuestGlyphSlotRec* slot, const char* function) {
    GuestDriverRec* driver = DriverOf(slot, function);
    GuestDriverClassPrefix* clazz = driver->clazz;
    GuestMemoryRec* memory = driver->root_memory;
    slot->library = driver->root_library;
    std::int32_t error = 0;
    auto* internal = static_cast<GuestSlotInternalRec*>(Allocate(memory, sizeof(GuestSlotInternalRec), &error, function));
    if (error) return error;
    slot->internal = internal;
    if (UsesOutlines(driver, function)) {
        auto* loader = static_cast<GuestGlyphLoaderRec*>(Allocate(memory, sizeof(GuestGlyphLoaderRec), &error, function));
        if (!error) {
            loader->memory = memory;
            internal->loader = loader;
        }
    }
    if (!error && clazz->init_slot) error = clazz->init_slot(slot);
    return error;
}

void SlotDone(GuestGlyphSlotRec* slot, const char* function) {
    GuestDriverRec* driver = DriverOf(slot, function);
    GuestDriverClassPrefix* clazz = driver->clazz;
    GuestMemoryRec* memory = driver->root_memory;
    if (clazz->done_slot) clazz->done_slot(slot);
    FreeBitmap(slot, function);
    GuestSlotInternalRec* internal = Internal(slot);
    if (!internal) return;
    if (UsesOutlines(driver, function)) {
        LoaderDone(static_cast<GuestGlyphLoaderRec*>(internal->loader), function);
        internal->loader = nullptr;
    }
    Free(memory, internal, function);
}

}

extern "C" {

int APS5_VABI FT_New_GlyphSlot_nid_postfix(GuestFaceRec* face, GuestGlyphSlotRec** aslot) {
    const char* function = __func__;
    if (!face || !face->driver) return InvalidArgument;
    auto* driver = static_cast<GuestDriverRec*>(face->driver);
    if (!driver->clazz) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    GuestMemoryRec* memory = driver->root_memory;
    const std::int64_t size = driver->clazz->slot_object_size;
    if (size >= 0 && size < static_cast<std::int64_t>(sizeof(GuestGlyphSlotRec))) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    std::int32_t error = 0;
    auto* slot = static_cast<GuestGlyphSlotRec*>(Allocate(memory, size, &error, function));
    if (error) {
        if (aslot) *aslot = nullptr;
        return error;
    }
    slot->face = face;
    error = SlotInit(slot, function);
    if (error) {
        SlotDone(slot, function);
        Free(memory, slot, function);
        return error;
    }
    slot->next = face->glyph;
    face->glyph = slot;
    if (aslot) *aslot = slot;
    return 0;
}

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
    const bool owned = (internal->flags & GlyphOwnBitmap) != 0;
    if (owned) {
        FreeOwned(memory, slot, function);
    } else {
        internal->flags |= GlyphOwnBitmap;
    }
    try {
        const auto bytes = static_cast<std::int64_t>(size);
        if (bytes > 0 && !memory) NotImplemented_nid_no_patch(function);
        std::int32_t error = 0;
        slot->bitmap.buffer = static_cast<unsigned char*>(OtMemAlloc(memory, bytes, &error));
        return error;
    } catch (...) {
        if (!owned) internal->flags &= ~GlyphOwnBitmap;
        throw;
    }
}

}
