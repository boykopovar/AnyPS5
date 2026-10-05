// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>

#include "prx/libSceFont/include/FontInternal.hpp"

namespace {

using namespace Font;

std::uint32_t EffectiveEdition(std::uint64_t edition) {
    return static_cast<std::uint32_t>(edition >> 32);
}

std::uint32_t* AcquireDeviceCache(FontLibNative* library) {
    auto* const busy = reinterpret_cast<std::uint32_t*>(std::numeric_limits<std::uintptr_t>::max());
    std::atomic_ref<std::uint32_t*> slot(library->device_cache_buf);
    for (;;) {
        std::uint32_t* current = slot.load(std::memory_order_acquire);
        if (current != busy && slot.compare_exchange_weak(current, busy, std::memory_order_acq_rel)) return current;
        Backoff();
    }
}

void ReleaseDeviceCache(FontLibNative* library, std::uint32_t* cache) {
    std::atomic_ref<std::uint32_t*>(library->device_cache_buf).store(cache, std::memory_order_release);
}

void* AcquireRendererSelection(RendererNative* renderer) {
    auto* const busy = reinterpret_cast<void*>(std::numeric_limits<std::uintptr_t>::max());
    std::atomic_ref<void*> slot(renderer->selection);
    for (;;) {
        void* current = slot.load(std::memory_order_acquire);
        if (current != busy && slot.compare_exchange_weak(current, busy, std::memory_order_acq_rel)) return current;
        Backoff();
    }
}

void* CreateFontContext(FontLibNative* library, std::uint32_t entries, std::uint32_t size) {
    void* ctx = library->iface->alloc(library->alloc_ctx, size);
    if (!ctx) return nullptr;
    std::memset(ctx, 0, size);
    auto* header = static_cast<FontCtxHeader*>(ctx);
    header->lock_word = 0;
    header->max_entries = entries;
    header->base = entries != 0 ? static_cast<std::uint8_t*>(ctx) + sizeof(FontCtxHeader) : nullptr;
    return ctx;
}

int SupportFonts(FontLibrary library, void* FontLibNative::*slot, std::uint32_t entries, std::uint32_t size, std::uint32_t formats) {
    auto* lib = static_cast<FontLibNative*>(library);
    if (!lib || lib->magic != LIBRARY_MAGIC) return SCE_FONT_ERROR_INVALID_LIBRARY;
    std::uint32_t previous = 0;
    if (!AcquireLibraryLock(lib, previous)) return SCE_FONT_ERROR_INVALID_LIBRARY;
    const auto finish = [&](int rc) {
        ReleaseLibraryLock(lib, previous);
        return rc;
    };
    if (lib->*slot) return finish(SCE_FONT_ERROR_ALREADY_SPECIFIED);
    void* ctx = CreateFontContext(lib, entries, size);
    if (!ctx) return finish(SCE_FONT_ERROR_ALLOCATION_FAILED);
    if (!lib->sys_driver || !lib->sys_driver->support_formats) {
        lib->iface->dealloc(lib->alloc_ctx, ctx);
        return finish(SCE_FONT_ERROR_INVALID_LIBRARY);
    }
    const int rc = lib->sys_driver->support_formats(lib, formats);
    if (rc != SCE_FONT_OK) {
        lib->iface->dealloc(lib->alloc_ctx, ctx);
        return finish(rc);
    }
    lib->*slot = ctx;
    return finish(SCE_FONT_OK);
}

}

#pragma GCC visibility push(default)

extern "C" {

int APS5_VABI sceFontMemoryInit(FontMemory* memory, void* regionAddress, std::uint32_t regionSize, const FontMemoryInterface* iface, void* mspaceObject, FontMemoryDestroyFunction destroyCallback, void* destroyObject) {
    if (!memory) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (!iface) {
        memory->mem_kind = 0;
        if (!regionAddress || regionSize == 0) return SCE_FONT_ERROR_INVALID_PARAMETER;
    }
    memory->mem_kind = MEMORY_MAGIC;
    memory->attr_bits = 0;
    memory->region_size = regionSize;
    memory->region_base = regionAddress;
    memory->mspace_handle = mspaceObject;
    memory->iface = iface;
    memory->on_destroy = destroyCallback;
    memory->destroy_ctx = destroyObject;
    memory->some_ctx1 = nullptr;
    memory->some_ctx2 = mspaceObject;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontMemoryTerm(FontMemory* memory) {
    if (!memory) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (memory->mem_kind != MEMORY_MAGIC) return SCE_FONT_ERROR_INVALID_MEMORY;
    if (static_cast<std::int8_t>(memory->attr_bits & 0xFF) < 0) {
        if (memory->iface && memory->iface->mspace_destroy) memory->iface->mspace_destroy(memory->some_ctx2, memory->mspace_handle);
        std::memset(memory, 0, sizeof(*memory));
        return SCE_FONT_OK;
    }
    if (memory->on_destroy) {
        memory->mem_kind = 0;
        memory->on_destroy(memory, memory->mspace_handle, memory->destroy_ctx);
        return SCE_FONT_OK;
    }
    std::memset(memory, 0, sizeof(*memory));
    return SCE_FONT_OK;
}

int APS5_VABI sceFontCreateLibraryWithEdition(const FontMemory* memory, const void* createParams, std::uint64_t edition, FontLibrary* pLibrary) {
    if (pLibrary) *pLibrary = nullptr;
    if (!memory) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (memory->mem_kind != MEMORY_MAGIC || !memory->iface || !memory->iface->alloc || !memory->iface->dealloc) return SCE_FONT_ERROR_INVALID_MEMORY;
    if (!createParams || !pLibrary) return SCE_FONT_ERROR_INVALID_PARAMETER;
    const auto allocFn = memory->iface->alloc;
    const auto freeFn = memory->iface->dealloc;
    auto* lib = static_cast<FontLibNative*>(allocFn(memory->mspace_handle, sizeof(FontLibNative)));
    if (!lib) return SCE_FONT_ERROR_ALLOCATION_FAILED;
    void* workspace = allocFn(memory->mspace_handle, 0x4000);
    if (!workspace) {
        freeFn(memory->mspace_handle, lib);
        return SCE_FONT_ERROR_ALLOCATION_FAILED;
    }
    std::memset(lib, 0, sizeof(FontLibNative));
    lib->mem_kind = MEMORY_MAGIC;
    lib->region_size = memory->region_size;
    lib->region_base = memory->region_base;
    lib->alloc_ctx = memory->mspace_handle;
    lib->iface = memory->iface;
    lib->alloc_fn = memory->iface->alloc;
    lib->dealloc_fn = memory->iface->dealloc;
    lib->realloc_fn = memory->iface->realloc_fn;
    lib->calloc_fn = memory->iface->calloc_fn;
    lib->sys_driver = static_cast<const SysDriver*>(createParams);
    lib->workspace_size = 0x4000;
    lib->workspace = workspace;
    lib->list_head_ptr = &lib->list_head;
    lib->list_head = nullptr;
    const auto initFn = lib->sys_driver->init;
    const int rc = initFn ? initFn(memory, lib) : SCE_FONT_ERROR_INVALID_PARAMETER;
    if (rc != SCE_FONT_OK) {
        freeFn(memory->mspace_handle, workspace);
        freeFn(memory->mspace_handle, lib);
        return rc;
    }
    const std::uint32_t version = EffectiveEdition(edition);
    if (version > 0x92FFFFu) {
        if (version < 0x1500000u) {
            lib->feature_word |= 1u | 2u | 4u;
        } else if (version < 0x2000000u) {
            lib->feature_word |= 2u | 4u;
        } else if (version <= 0x2000070u) {
            lib->feature_word |= 4u;
        }
    }
    lib->magic = LIBRARY_MAGIC;
    *pLibrary = lib;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontCreateLibrary(const FontMemory* memory, const void* createParams, FontLibrary* pLibrary) {
    return sceFontCreateLibraryWithEdition(memory, createParams, 0, pLibrary);
}

int APS5_VABI sceFontDestroyLibrary(FontLibrary* pLibrary) {
    if (!pLibrary) return SCE_FONT_ERROR_INVALID_PARAMETER;
    auto* lib = static_cast<FontLibNative*>(*pLibrary);
    if (!lib || lib->magic != LIBRARY_MAGIC) return SCE_FONT_ERROR_INVALID_LIBRARY;
    if (lib->sys_driver && lib->sys_driver->term) lib->sys_driver->term(lib);
    const auto freeFn = lib->iface->dealloc;
    void* allocCtx = lib->alloc_ctx;
    if ((lib->flags & 1u) != 0 && lib->device_cache_buf) freeFn(allocCtx, lib->device_cache_buf);
    if (lib->external_fonts_ctx) freeFn(allocCtx, lib->external_fonts_ctx);
    if (lib->sysfonts_ctx) freeFn(allocCtx, lib->sysfonts_ctx);
    if (lib->workspace) freeFn(allocCtx, lib->workspace);
    lib->magic = 0;
    freeFn(allocCtx, lib);
    *pLibrary = nullptr;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontCreateRendererWithEdition(const FontMemory* memory, const void* createParams, std::uint64_t edition, FontRenderer* pRenderer) {
    if (!memory) {
        if (pRenderer) *pRenderer = nullptr;
        return SCE_FONT_ERROR_INVALID_PARAMETER;
    }
    int rc = SCE_FONT_ERROR_INVALID_MEMORY;
    if (memory->mem_kind == MEMORY_MAGIC && memory->iface && memory->iface->alloc && memory->iface->dealloc) {
        rc = SCE_FONT_ERROR_INVALID_PARAMETER;
        if (createParams && pRenderer) {
            const auto allocFn = memory->iface->alloc;
            const auto freeFn = memory->iface->dealloc;
            const auto* selection = static_cast<const RendererSelection*>(createParams);
            void* rendererMemory = allocFn(memory->mspace_handle, selection->size);
            void* workspace = allocFn(memory->mspace_handle, 0x4000);
            rc = SCE_FONT_ERROR_ALLOCATION_FAILED;
            if (rendererMemory && workspace) {
                auto* renderer = static_cast<RendererNative*>(rendererMemory);
                std::memset(renderer, 0, offsetof(RendererNative, mem_kind));
                renderer->magic = RENDERER_MAGIC;
                renderer->mem_kind = MEMORY_MAGIC;
                renderer->region_size = memory->region_size;
                renderer->region_base = memory->region_base;
                renderer->alloc_ctx = memory->mspace_handle;
                renderer->mem_iface = memory->iface;
                renderer->alloc_fn = memory->iface->alloc;
                renderer->free_fn = memory->iface->dealloc;
                renderer->realloc_fn = memory->iface->realloc_fn;
                renderer->calloc_fn = memory->iface->calloc_fn;
                renderer->selection = const_cast<void*>(createParams);
                renderer->outline_magic_0x90 = 0x400000000000ull;
                renderer->workspace = workspace;
                renderer->workspace_size = 0x4000;
                renderer->reserved_a8 = 0;
                renderer->outline_policy_flag = 0;
                rc = selection->create_fn ? selection->create_fn(renderer) : SCE_FONT_ERROR_FATAL;
                if (rc == SCE_FONT_OK) {
                    if (EffectiveEdition(edition) - 0x930000u < 0x1BD0000u) {
                        renderer->feature_byte_0x0c |= 1u;
                        renderer->outline_policy_flag = 1;
                    }
                    *pRenderer = renderer;
                    return SCE_FONT_OK;
                }
            }
            if (workspace) freeFn(memory->mspace_handle, workspace);
            if (rendererMemory) freeFn(memory->mspace_handle, rendererMemory);
        }
    }
    if (pRenderer) *pRenderer = nullptr;
    return rc;
}

int APS5_VABI sceFontCreateRenderer(const FontMemory* memory, const void* createParams, FontRenderer* pRenderer) {
    return sceFontCreateRendererWithEdition(memory, createParams, 0, pRenderer);
}

int APS5_VABI sceFontDestroyRenderer(FontRenderer* pRenderer) {
    if (!pRenderer) return SCE_FONT_ERROR_INVALID_PARAMETER;
    auto* renderer = static_cast<RendererNative*>(*pRenderer);
    if (!renderer || renderer->magic != RENDERER_MAGIC) return SCE_FONT_ERROR_INVALID_RENDERER;
    const auto* selection = static_cast<const RendererSelection*>(AcquireRendererSelection(renderer));
    int rc = SCE_FONT_ERROR_FATAL;
    if (selection && selection->destroy_fn) rc = selection->destroy_fn(renderer);
    renderer->selection = nullptr;
    const auto freeFn = renderer->free_fn;
    void* allocCtx = renderer->alloc_ctx;
    if (renderer->workspace) freeFn(allocCtx, renderer->workspace);
    freeFn(allocCtx, renderer);
    *pRenderer = nullptr;
    return rc;
}

int APS5_VABI sceFontRendererGetOutlineBufferSize(FontRenderer fontRenderer, std::uint32_t* size) {
    if (!size) return SCE_FONT_ERROR_INVALID_PARAMETER;
    *size = 0;
    auto* renderer = static_cast<RendererNative*>(fontRenderer);
    if (!renderer || renderer->magic != RENDERER_MAGIC) return SCE_FONT_ERROR_INVALID_RENDERER;
    *size = static_cast<std::uint32_t>(renderer->workspace_size);
    return SCE_FONT_OK;
}

int APS5_VABI sceFontRendererResetOutlineBuffer(FontRenderer fontRenderer) {
    auto* renderer = static_cast<RendererNative*>(fontRenderer);
    if (!renderer || renderer->magic != RENDERER_MAGIC) return SCE_FONT_ERROR_INVALID_RENDERER;
    if (renderer->workspace && renderer->workspace_size) std::memset(renderer->workspace, 0, static_cast<std::size_t>(renderer->workspace_size));
    return SCE_FONT_OK;
}

int APS5_VABI sceFontRendererSetOutlineBufferPolicy(FontRenderer fontRenderer, std::uint64_t bufferPolicy, std::uint32_t basalSize, std::uint32_t limitSize) {
    (void)bufferPolicy;
    auto* renderer = static_cast<RendererNative*>(fontRenderer);
    if (!renderer || renderer->magic != RENDERER_MAGIC) return SCE_FONT_ERROR_INVALID_RENDERER;
    if (limitSize != 0 && basalSize > limitSize) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (!renderer->alloc_fn || !renderer->free_fn || !renderer->alloc_ctx) return SCE_FONT_ERROR_INVALID_MEMORY;
    auto desiredSize = std::max(static_cast<std::uint64_t>(renderer->workspace_size), static_cast<std::uint64_t>(basalSize));
    if (limitSize != 0) desiredSize = std::min(desiredSize, static_cast<std::uint64_t>(limitSize));
    if (desiredSize == 0) desiredSize = 0x4000;
    if (!renderer->workspace || renderer->workspace_size != desiredSize) {
        void* workspace = renderer->alloc_fn(renderer->alloc_ctx, static_cast<std::uint32_t>(desiredSize));
        if (!workspace) return SCE_FONT_ERROR_ALLOCATION_FAILED;
        if (renderer->workspace) renderer->free_fn(renderer->alloc_ctx, renderer->workspace);
        renderer->workspace = workspace;
        renderer->workspace_size = desiredSize;
    }
    return SCE_FONT_OK;
}

int APS5_VABI sceFontGetPixelResolution(FontLibrary library, std::uint32_t* subPixelCount) {
    if (!subPixelCount) return SCE_FONT_ERROR_INVALID_PARAMETER;
    *subPixelCount = 0;
    const auto* lib = static_cast<const FontLibNative*>(library);
    if (!lib || lib->magic != LIBRARY_MAGIC || !lib->sys_driver || !lib->sys_driver->pixel_resolution) return SCE_FONT_ERROR_INVALID_LIBRARY;
    *subPixelCount = lib->sys_driver->pixel_resolution();
    return SCE_FONT_OK;
}

int APS5_VABI sceFontAttachDeviceCacheBuffer(FontLibrary library, void* buffer, std::uint32_t size) {
    auto* lib = static_cast<FontLibNative*>(library);
    if (!lib || lib->magic != LIBRARY_MAGIC) return SCE_FONT_ERROR_INVALID_LIBRARY;
    std::uint32_t previous = 0;
    if (!AcquireLibraryLock(lib, previous)) return SCE_FONT_ERROR_INVALID_LIBRARY;
    std::uint32_t* current = AcquireDeviceCache(lib);
    std::uint32_t* stored = current;
    int rc;
    if (current) {
        rc = SCE_FONT_ERROR_ALREADY_ATTACHED;
    } else if (size < 0x1020) {
        rc = SCE_FONT_ERROR_INVALID_PARAMETER;
    } else {
        auto* header = static_cast<std::uint32_t*>(buffer);
        std::uint32_t owned = 0;
        if (!header) {
            header = static_cast<std::uint32_t*>(lib->iface->alloc(lib->alloc_ctx, size));
            owned = 1;
        }
        if (!header) {
            rc = SCE_FONT_ERROR_ALLOCATION_FAILED;
        } else {
            const std::uint32_t pageCount = (size - 0x1000) >> 12;
            header[0] = size;
            header[1] = pageCount;
            header[2] = 0;
            header[3] = pageCount;
            const std::uint64_t pageInfo = 0x0FF800001000ull;
            std::memcpy(header + 4, &pageInfo, sizeof(pageInfo));
            if (size - 0x1000 > 0x0FFF) {
                lib->flags |= owned;
                stored = header;
                rc = SCE_FONT_OK;
            } else {
                if (owned) lib->iface->dealloc(lib->alloc_ctx, header);
                rc = SCE_FONT_ERROR_INVALID_PARAMETER;
            }
        }
    }
    ReleaseDeviceCache(lib, stored);
    ReleaseLibraryLock(lib, previous);
    return rc;
}

int APS5_VABI sceFontClearDeviceCache(FontLibrary library) {
    auto* lib = static_cast<FontLibNative*>(library);
    if (!lib || lib->magic != LIBRARY_MAGIC) return SCE_FONT_ERROR_INVALID_LIBRARY;
    std::uint32_t previous = 0;
    if (!AcquireLibraryLock(lib, previous)) return SCE_FONT_ERROR_INVALID_LIBRARY;
    std::uint32_t* current = AcquireDeviceCache(lib);
    int rc = SCE_FONT_ERROR_NOT_ATTACHED_CACHE_BUFFER;
    if (current) {
        current[3] = current[1];
        current[2] = 0;
        rc = SCE_FONT_OK;
    }
    ReleaseDeviceCache(lib, current);
    ReleaseLibraryLock(lib, previous);
    return rc;
}

int APS5_VABI sceFontSupportExternalFonts(FontLibrary library, std::uint32_t fontMax, std::uint32_t formats) {
    return SupportFonts(library, &FontLibNative::external_fonts_ctx, fontMax, (fontMax << 6) | 0x20u, formats);
}

int APS5_VABI sceFontSupportSystemFonts(FontLibrary library) {
    return SupportFonts(library, &FontLibNative::sysfonts_ctx, 0x40, 0x1020, 0x52);
}

int APS5_VABI sceFontSetFontsOpenMode(FontLibrary library, std::uint32_t openMode) {
    auto* lib = static_cast<FontLibNative*>(library);
    if (!lib || lib->magic != LIBRARY_MAGIC) return SCE_FONT_ERROR_INVALID_LIBRARY;
    if (openMode > 2) return SCE_FONT_ERROR_INVALID_PARAMETER;
    return SCE_FONT_OK;
}

}

#pragma GCC visibility pop
