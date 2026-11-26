// Derived from FreeType src/base/ftobjs.c (FreeType 2.4.12, commit cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9).
// Copyright 1996-2013 by David Turner, Robert Wilhelm, and Werner Lemberg.
// This file is part of the FreeType project, and may only be used, modified, and distributed
// under the terms of the FreeType project license, LICENSE.TXT. By continuing to use, modify,
// or distribute this file you indicate that you have read the license and understand and accept it fully.

#include "prx/libSceFreeTypeOt/include/GlyphSlot.hpp"
#include "prx/libSceFreeTypeOt/include/Memory.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstdint>

using namespace FreeType;

namespace {

constexpr int InvalidArgument = 0x06;

bool CopySlotIn(GuestGlyphSlotRec* slot, GuestGlyphSlotRec& guestSlot) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&guestSlot, slot, sizeof(guestSlot));
    errno = savedErrno;
    return copied;
}

GuestSlotInternalRec* Internal(const GuestGlyphSlotRec& slot) {
    return static_cast<GuestSlotInternalRec*>(slot.internal);
}

GuestMemoryRec* FaceMemory(const GuestGlyphSlotRec& slot, const char* function) {
    if (!slot.face) NotImplemented_nid_no_patch(function);
    GuestFaceRec guestFace{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&guestFace, slot.face, sizeof(guestFace));
    errno = savedErrno;
    if (!copied) return nullptr;
    return static_cast<GuestMemoryRec*>(guestFace.memory);
}

bool FreeOwned(GuestMemoryRec* memory, GuestGlyphSlotRec* slot, GuestGlyphSlotRec& guestSlot, const char* function) {
    if (guestSlot.bitmap.buffer && !memory) NotImplemented_nid_no_patch(function);
    std::uint8_t* cleared = nullptr;
    const int savedErrno = errno;
    const bool written = GuestCopyOut_nid_no_patch(&slot->bitmap.buffer, &cleared, sizeof(cleared));
    errno = savedErrno;
    if (!written) return false;
    OtMemFree(memory, guestSlot.bitmap.buffer);
    guestSlot.bitmap.buffer = nullptr;
    return true;
}

bool FreeBitmap(GuestGlyphSlotRec* slot, GuestGlyphSlotRec& guestSlot, const char* function) {
    GuestSlotInternalRec* internal = Internal(guestSlot);
    if (internal && (internal->flags & GlyphOwnBitmap)) {
        GuestMemoryRec* memory = FaceMemory(guestSlot, function);
        if (guestSlot.bitmap.buffer && !memory) NotImplemented_nid_no_patch(function);
        std::uint8_t* cleared = nullptr;
        const int savedErrno = errno;
        const bool written = GuestCopyOut_nid_no_patch(&slot->bitmap.buffer, &cleared, sizeof(cleared));
        errno = savedErrno;
        if (!written) return false;
        OtMemFree(memory, guestSlot.bitmap.buffer);
        guestSlot.bitmap.buffer = nullptr;
        internal->flags &= ~GlyphOwnBitmap;
        return true;
    }
    std::uint8_t* cleared = nullptr;
    const int savedErrno = errno;
    const bool written = GuestCopyOut_nid_no_patch(&slot->bitmap.buffer, &cleared, sizeof(cleared));
    errno = savedErrno;
    if (written) guestSlot.bitmap.buffer = nullptr;
    return written;
}

bool DriverOf(
    const GuestGlyphSlotRec& guestSlot,
    GuestFaceRec& guestFace,
    GuestDriverRec& guestDriver,
    GuestDriverClassPrefix& guestClazz) {
    if (!guestSlot.face) return false;
    const int faceErrno = errno;
    const bool faceCopied = GuestCopyIn_nid_no_patch(&guestFace, guestSlot.face, sizeof(guestFace));
    errno = faceErrno;
    if (!faceCopied || !guestFace.driver) return false;
    const int driverErrno = errno;
    const bool driverCopied = GuestCopyIn_nid_no_patch(&guestDriver, guestFace.driver, sizeof(guestDriver));
    errno = driverErrno;
    if (!driverCopied || !guestDriver.clazz) return false;
    const int clazzErrno = errno;
    const bool clazzCopied = GuestCopyIn_nid_no_patch(&guestClazz, guestDriver.clazz, sizeof(guestClazz));
    errno = clazzErrno;
    return clazzCopied;
}

bool UsesOutlines(const GuestDriverRec& guestDriver, bool& usesOutlines, const char* function) {
    if (!guestDriver.root_clazz) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    GuestModuleClass rootClazz{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&rootClazz, guestDriver.root_clazz, sizeof(rootClazz));
    errno = savedErrno;
    if (!copied) return false;
    usesOutlines = !(rootClazz.module_flags & ModuleDriverNoOutlines);
    return true;
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
    GuestGlyphLoaderRec guestLoader{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&guestLoader, loader, sizeof(guestLoader));
    errno = savedErrno;
    if (!copied) return;
    GuestMemoryRec* memory = guestLoader.memory;
    Free(memory, guestLoader.base.outline.points, function);
    Free(memory, guestLoader.base.outline.tags, function);
    Free(memory, guestLoader.base.outline.contours, function);
    Free(memory, guestLoader.base.extra_points, function);
    Free(memory, guestLoader.base.subglyphs, function);
    Free(memory, loader, function);
}

std::int32_t SlotInit(GuestGlyphSlotRec* slot, const char* function) {
    GuestGlyphSlotRec guestSlot{};
    if (!slot || !CopySlotIn(slot, guestSlot)) return InvalidArgument;
    GuestFaceRec guestFace{};
    GuestDriverRec guestDriver{};
    GuestDriverClassPrefix guestClazz{};
    if (!DriverOf(guestSlot, guestFace, guestDriver, guestClazz)) return InvalidArgument;
    bool usesOutlines = false;
    if (!UsesOutlines(guestDriver, usesOutlines, function)) return InvalidArgument;
    GuestMemoryRec* memory = guestDriver.root_memory;
    slot->library = guestDriver.root_library;
    std::int32_t error = 0;
    auto* internal = static_cast<GuestSlotInternalRec*>(Allocate(memory, sizeof(GuestSlotInternalRec), &error, function));
    if (error) return error;
    slot->internal = internal;
    if (usesOutlines) {
        auto* loader = static_cast<GuestGlyphLoaderRec*>(Allocate(memory, sizeof(GuestGlyphLoaderRec), &error, function));
        if (!error) {
            loader->memory = memory;
            internal->loader = loader;
        }
    }
    if (!error && guestClazz.init_slot) error = guestClazz.init_slot(slot);
    return error;
}

void SlotDoneWithDriver(
    GuestGlyphSlotRec* slot,
    const GuestDriverRec& guestDriver,
    const GuestDriverClassPrefix& guestClazz,
    const char* function) {
    GuestGlyphSlotRec guestSlot{};
    if (!slot || !CopySlotIn(slot, guestSlot)) return;
    GuestMemoryRec* memory = guestDriver.root_memory;
    if (guestClazz.done_slot) guestClazz.done_slot(slot);
    FreeBitmap(slot, guestSlot, function);
    GuestSlotInternalRec* internal = Internal(guestSlot);
    if (!internal) return;
    bool usesOutlines = false;
    if (UsesOutlines(guestDriver, usesOutlines, function) && usesOutlines) {
        GuestSlotInternalRec guestInternal{};
        const int readErrno = errno;
        const bool copied = GuestCopyIn_nid_no_patch(&guestInternal, internal, sizeof(guestInternal));
        errno = readErrno;
        if (copied) {
            LoaderDone(static_cast<GuestGlyphLoaderRec*>(guestInternal.loader), function);
            void* clearedLoader = nullptr;
            const int writeErrno = errno;
            GuestCopyOut_nid_no_patch(&internal->loader, &clearedLoader, sizeof(clearedLoader));
            errno = writeErrno;
        }
    }
    Free(memory, internal, function);
    void* clearedInternal = nullptr;
    const int writeErrno = errno;
    GuestCopyOut_nid_no_patch(&slot->internal, &clearedInternal, sizeof(clearedInternal));
    errno = writeErrno;
}

void RollbackSlot(
    GuestFaceRec* face,
    GuestGlyphSlotRec* slot,
    const GuestDriverRec& guestDriver,
    const GuestDriverClassPrefix& guestClazz,
    const char* function) {
    const int savedErrno = errno;
    GuestCopyOut_nid_no_patch(&slot->face, &face, sizeof(face));
    errno = savedErrno;
    SlotDoneWithDriver(slot, guestDriver, guestClazz, function);
    Free(guestDriver.root_memory, slot, function);
}

}

extern "C" {

int APS5_VABI FT_New_GlyphSlot_nid_postfix(GuestFaceRec* face, GuestGlyphSlotRec** aslot) {
    const char* function = __func__;
    if (!face) return InvalidArgument;
    GuestFaceRec guestFace{};
    const int faceErrno = errno;
    const bool faceCopied = GuestCopyIn_nid_no_patch(&guestFace, face, sizeof(guestFace));
    errno = faceErrno;
    if (!faceCopied || !guestFace.driver) return InvalidArgument;
    auto* driver = static_cast<GuestDriverRec*>(guestFace.driver);
    GuestDriverRec guestDriver{};
    const int driverErrno = errno;
    const bool driverCopied = GuestCopyIn_nid_no_patch(&guestDriver, driver, sizeof(guestDriver));
    errno = driverErrno;
    if (!driverCopied) return InvalidArgument;
    if (!guestDriver.clazz) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    GuestDriverClassPrefix guestClazz{};
    const int clazzErrno = errno;
    const bool clazzCopied = GuestCopyIn_nid_no_patch(&guestClazz, guestDriver.clazz, sizeof(guestClazz));
    errno = clazzErrno;
    if (!clazzCopied) return InvalidArgument;
    GuestMemoryRec* memory = guestDriver.root_memory;
    const std::int64_t size = guestClazz.slot_object_size;
    if (size >= 0 && size < static_cast<std::int64_t>(sizeof(GuestGlyphSlotRec))) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    std::int32_t error = 0;
    auto* slot = static_cast<GuestGlyphSlotRec*>(Allocate(memory, size, &error, function));
    if (error) {
        if (aslot) {
            GuestGlyphSlotRec* cleared = nullptr;
            const int clearErrno = errno;
            const bool clearedOk = GuestCopyOut_nid_no_patch(aslot, &cleared, sizeof(cleared));
            errno = clearErrno;
            if (!clearedOk) return InvalidArgument;
        }
        return error;
    }
    slot->face = face;
    error = SlotInit(slot, function);
    if (error) {
        RollbackSlot(face, slot, guestDriver, guestClazz, function);
        return error;
    }
    GuestGlyphSlotRec* currentHead = guestFace.glyph;
    const int headErrno = errno;
    if (!GuestCopyIn_nid_no_patch(&currentHead, &face->glyph, sizeof(currentHead))) {
        errno = headErrno;
        RollbackSlot(face, slot, guestDriver, guestClazz, function);
        return InvalidArgument;
    }
    errno = headErrno;
    slot->next = currentHead;
    const int glyphErrno = errno;
    const bool glyphWritten = GuestCopyOut_nid_no_patch(&face->glyph, &slot, sizeof(slot));
    errno = glyphErrno;
    if (!glyphWritten) {
        RollbackSlot(face, slot, guestDriver, guestClazz, function);
        return InvalidArgument;
    }
    if (aslot) {
        const int outErrno = errno;
        const bool outWritten = GuestCopyOut_nid_no_patch(aslot, &slot, sizeof(slot));
        errno = outErrno;
        if (!outWritten) {
            GuestGlyphSlotRec* slotNext = nullptr;
            GuestGlyphSlotRec* listHead = nullptr;
            const int restoreErrno = errno;
            const bool readNext = GuestCopyIn_nid_no_patch(&slotNext, &slot->next, sizeof(slotNext));
            const bool readHead = GuestCopyIn_nid_no_patch(&listHead, &face->glyph, sizeof(listHead));
            errno = restoreErrno;
            if (readNext && readHead) {
                if (listHead == slot) {
                    GuestCopyOut_nid_no_patch(&face->glyph, &slotNext, sizeof(slotNext));
                    errno = restoreErrno;
                } else {
                    GuestGlyphSlotRec* prev = listHead;
                    while (prev) {
                        GuestGlyphSlotRec* next = nullptr;
                        if (!GuestCopyIn_nid_no_patch(&next, &prev->next, sizeof(next))) break;
                        if (next == slot) {
                            GuestCopyOut_nid_no_patch(&prev->next, &slotNext, sizeof(slotNext));
                            break;
                        }
                        prev = next;
                    }
                    errno = restoreErrno;
                }
            }
            RollbackSlot(face, slot, guestDriver, guestClazz, function);
            return InvalidArgument;
        }
    }
    return 0;
}

void APS5_VABI ft_glyphslot_free_bitmap_nid_postfix(GuestGlyphSlotRec* slot) {
    if (!slot) NotImplemented_nid_no_patch(__func__);
    GuestGlyphSlotRec guestSlot{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&guestSlot, slot, sizeof(guestSlot));
    errno = savedErrno;
    if (!copied) return;
    FreeBitmap(slot, guestSlot, __func__);
}

void APS5_VABI ft_glyphslot_set_bitmap_nid_postfix(GuestGlyphSlotRec* slot, std::uint8_t* buffer) {
    if (!slot) NotImplemented_nid_no_patch(__func__);
    GuestGlyphSlotRec guestSlot{};
    const int readErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&guestSlot, slot, sizeof(guestSlot));
    errno = readErrno;
    if (!copied) return;
    GuestSlotInternalRec* internal = Internal(guestSlot);
    const bool owned = internal && (internal->flags & GlyphOwnBitmap);
    GuestMemoryRec* memory = owned ? FaceMemory(guestSlot, __func__) : nullptr;
    if (owned && guestSlot.bitmap.buffer && !memory) NotImplemented_nid_no_patch(__func__);
    const int writeErrno = errno;
    const bool written = GuestCopyOut_nid_no_patch(&slot->bitmap.buffer, &buffer, sizeof(buffer));
    errno = writeErrno;
    if (!written) return;
    if (owned) {
        OtMemFree(memory, guestSlot.bitmap.buffer);
        internal->flags &= ~GlyphOwnBitmap;
    }
}

int APS5_VABI ft_glyphslot_alloc_bitmap_nid_postfix(GuestGlyphSlotRec* slot, std::uint64_t size) {
    const char* function = __func__;
    if (!slot) NotImplemented_nid_no_patch(function);
    GuestGlyphSlotRec guestSlot{};
    const int readErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&guestSlot, slot, sizeof(guestSlot));
    errno = readErrno;
    if (!copied) return InvalidArgument;
    if (!guestSlot.internal) NotImplemented_nid_no_patch(function);
    GuestMemoryRec* memory = FaceMemory(guestSlot, function);
    GuestSlotInternalRec* internal = Internal(guestSlot);
    if (internal->flags & GlyphOwnBitmap) {
        if (!FreeOwned(memory, slot, guestSlot, function)) return InvalidArgument;
    } else {
        internal->flags |= GlyphOwnBitmap;
    }
    const auto bytes = static_cast<std::int64_t>(size);
    if (bytes > 0 && !memory) NotImplemented_nid_no_patch(function);
    std::int32_t error = 0;
    auto* allocated = static_cast<std::uint8_t*>(OtMemAlloc(memory, bytes, &error));
    const int writeErrno = errno;
    const bool written = GuestCopyOut_nid_no_patch(&slot->bitmap.buffer, &allocated, sizeof(allocated));
    errno = writeErrno;
    if (!written) {
        if (allocated) OtMemFree(memory, allocated);
        return InvalidArgument;
    }
    return error;
}

}
