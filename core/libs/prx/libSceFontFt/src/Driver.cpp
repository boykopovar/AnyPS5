#include "prx/libSceFontFt/include/Api.hpp"
// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#ifdef _WIN32
#include "prx/libc/include/PreciseWait.hpp"
#include <windows.h>
#endif

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_SYSTEM_H
#include FT_TRUETYPE_TABLES_H

#include "prx/libSceFont/include/FontFreeType.hpp"
#include "prx/libSceFontFt/include/FontFtDriver.hpp"
#include "prx/libc/include/General.hpp"

namespace {

using namespace Font;

template <typename T>
bool CopyInGuest(T& destination, const T* source) {
    if (!source) return false;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&destination, source, sizeof(T));
    errno = savedErrno;
    return copied;
}

template <typename T>
bool CopyOutGuest(T* destination, const T& value) {
    if (!destination) return false;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, &value, sizeof(T));
    errno = savedErrno;
    return copied;
}

bool CopyOutGuestBytes(void* destination, const void* source, std::size_t bytes) {
    if (bytes == 0) return true;
    if (!destination) return false;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

int CopyPathFromGuest(const char* source, std::string& destination) {
    const int savedErrno = errno;
    const int rc = CopyGuestPath_nid_no_patch(source, destination);
    errno = savedErrno;
    return rc;
}

struct FtLibraryContext {
    void* allocCtx;
    const FontMemoryInterface* iface;
    FontMemoryInterface ifaceStorage;
    FT_Memory memory;
    FT_Library library;
    FT_Memory hostMemory;
    FT_Library hostLibrary;
};

void* FtAlloc(FT_Memory memory, long size) {
    if (!memory || size <= 0 || !FontFt::FitsGuestSize(size)) return nullptr;
    auto* ctx = static_cast<FtLibraryContext*>(memory->user);
    if (!ctx || !ctx->iface || !ctx->iface->alloc) return nullptr;
    return ctx->iface->alloc(ctx->allocCtx, static_cast<std::uint32_t>(size));
}

void FtFree(FT_Memory memory, void* block) {
    if (!memory || !block) return;
    auto* ctx = static_cast<FtLibraryContext*>(memory->user);
    if (!ctx || !ctx->iface || !ctx->iface->dealloc) return;
    ctx->iface->dealloc(ctx->allocCtx, block);
}

void* FtRealloc(FT_Memory memory, long currentSize, long newSize, void* block) {
    if (!memory) return nullptr;
    auto* ctx = static_cast<FtLibraryContext*>(memory->user);
    if (!ctx || !ctx->iface) return nullptr;
    if (newSize > 0 && !FontFt::FitsGuestSize(newSize)) return nullptr;
    if (ctx->iface->realloc_fn) return ctx->iface->realloc_fn(ctx->allocCtx, block, static_cast<std::uint32_t>(newSize));
    if (newSize <= 0) {
        FtFree(memory, block);
        return nullptr;
    }
    void* result = FtAlloc(memory, newSize);
    if (!result) return nullptr;
    if (block && currentSize > 0) std::memcpy(result, block, static_cast<std::size_t>(std::min(currentSize, newSize)));
    FtFree(memory, block);
    return result;
}

void* HostAlloc(FT_Memory, long size) {
    return size > 0 ? std::malloc(static_cast<std::size_t>(size)) : nullptr;
}

void HostFree(FT_Memory, void* block) {
    std::free(block);
}

void* HostRealloc(FT_Memory, long, long newSize, void* block) {
    if (newSize <= 0) {
        std::free(block);
        return nullptr;
    }
    return std::realloc(block, static_cast<std::size_t>(newSize));
}

FT_Library SystemFontLibrary(FtLibraryContext* ctx) {
    if (ctx->hostLibrary) return ctx->hostLibrary;
    auto* hostMemory = static_cast<FT_Memory>(ctx->iface->alloc(ctx->allocCtx, sizeof(FT_MemoryRec_)));
    if (!hostMemory) return nullptr;
    std::memset(hostMemory, 0, sizeof(*hostMemory));
    hostMemory->user = ctx;
    hostMemory->alloc = &HostAlloc;
    hostMemory->free = &HostFree;
    hostMemory->realloc = &HostRealloc;
    FT_Library hostLibrary = nullptr;
    if (FT_New_Library(hostMemory, &hostLibrary) != 0 || !hostLibrary) {
        ctx->iface->dealloc(ctx->allocCtx, hostMemory);
        return nullptr;
    }
    FT_Add_Default_Modules(hostLibrary);
    ctx->hostMemory = hostMemory;
    ctx->hostLibrary = hostLibrary;
    return hostLibrary;
}

void WriteU16(std::uint8_t* out, std::size_t offset, std::uint16_t value) {
    std::memcpy(out + offset, &value, sizeof(value));
}

void WriteU32(std::uint8_t* out, std::size_t offset, std::uint32_t value) {
    std::memcpy(out + offset, &value, sizeof(value));
}

void WriteS32(std::uint8_t* out, std::size_t offset, std::int32_t value) {
    std::memcpy(out + offset, &value, sizeof(value));
}

void WritePointer(std::uint8_t* out, std::size_t offset, const void* value) {
    std::memcpy(out + offset, &value, sizeof(value));
}

float FromF26Dot6(std::int64_t value) {
    return static_cast<float>(value) * ONE_OVER_64;
}

FT_Face FaceOf(const FontObj* obj) {
    return static_cast<FT_Face>(obj->ft_face);
}

std::uint32_t APS5_VABI LibraryGetPixelResolution() {
    return 0x40;
}

int APS5_VABI LibraryInit(const FontMemory* memory, FontLibNative* library) {
    FontMemory localMemory{};
    FontLibNative localLibrary{};
    if (!CopyInGuest(localMemory, memory) || !CopyInGuest(localLibrary, library)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    FontMemoryInterface localIface{};
    if (!localMemory.iface || !CopyInGuest(localIface, localMemory.iface) || !localIface.alloc || !localIface.dealloc) {
        return SCE_FONT_ERROR_INVALID_MEMORY;
    }
    const auto allocFn = localIface.alloc;
    const auto freeFn = localIface.dealloc;
    void* allocCtx = localMemory.mspace_handle;
    auto* ctx = static_cast<FtLibraryContext*>(allocFn(allocCtx, sizeof(FtLibraryContext)));
    if (!ctx) return SCE_FONT_ERROR_ALLOCATION_FAILED;
    std::memset(ctx, 0, sizeof(FtLibraryContext));
    ctx->allocCtx = allocCtx;
    ctx->ifaceStorage = localIface;
    ctx->iface = &ctx->ifaceStorage;
    auto* ftMemory = static_cast<FT_Memory>(allocFn(allocCtx, sizeof(FT_MemoryRec_)));
    if (!ftMemory) {
        freeFn(allocCtx, ctx);
        return SCE_FONT_ERROR_ALLOCATION_FAILED;
    }
    std::memset(ftMemory, 0, sizeof(*ftMemory));
    ftMemory->user = ctx;
    ftMemory->alloc = &FtAlloc;
    ftMemory->free = &FtFree;
    ftMemory->realloc = &FtRealloc;
    ctx->memory = ftMemory;
    FT_Library ftLibrary = nullptr;
    if (FT_New_Library(ftMemory, &ftLibrary) != 0 || !ftLibrary) {
        freeFn(allocCtx, ftMemory);
        freeFn(allocCtx, ctx);
        return SCE_FONT_ERROR_ALLOCATION_FAILED;
    }
    FT_Add_Default_Modules(ftLibrary);
    ctx->library = ftLibrary;
    localLibrary.fontset_registry = ctx;
    localLibrary.flags = 0x60000000;
    if (!CopyOutGuest(library, localLibrary)) {
        FT_Done_Library(ftLibrary);
        freeFn(allocCtx, ftMemory);
        freeFn(allocCtx, ctx);
        return SCE_FONT_ERROR_INVALID_PARAMETER;
    }
    return SCE_FONT_OK;
}

int APS5_VABI LibraryTerm(FontLibNative* library) {
    FontLibNative localLibrary{};
    if (!CopyInGuest(localLibrary, library)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    FontMemoryInterface localIface{};
    const auto freeFn = (localLibrary.iface && CopyInGuest(localIface, localLibrary.iface)) ? localIface.dealloc : nullptr;
    if (!freeFn) return SCE_FONT_ERROR_INVALID_PARAMETER;
    auto* ctx = static_cast<FtLibraryContext*>(localLibrary.fontset_registry);
    if (!ctx) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (ctx->library) {
        FT_Done_Library(ctx->library);
        ctx->library = nullptr;
    }
    if (ctx->hostLibrary) {
        FT_Done_Library(ctx->hostLibrary);
        ctx->hostLibrary = nullptr;
    }
    if (ctx->hostMemory) {
        freeFn(localLibrary.alloc_ctx, ctx->hostMemory);
        ctx->hostMemory = nullptr;
    }
    if (ctx->memory) {
        freeFn(localLibrary.alloc_ctx, ctx->memory);
        ctx->memory = nullptr;
    }
    freeFn(localLibrary.alloc_ctx, ctx);
    localLibrary.fontset_registry = nullptr;
    if (!CopyOutGuest(library, localLibrary)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    return SCE_FONT_OK;
}

int APS5_VABI LibrarySupport(FontLibNative* library, std::uint32_t formats) {
    (void)formats;
    FontLibNative localLibrary{};
    if (!CopyInGuest(localLibrary, library) || !localLibrary.fontset_registry) return SCE_FONT_ERROR_INVALID_PARAMETER;
    return SCE_FONT_OK;
}

#ifdef _WIN32
void CloseMappedFile(FT_Stream stream) {
    UnmapViewOfFile(stream->base);
    delete stream;
}

FT_Error OpenFileFace(FT_Library library, const char* path, FT_Long subFontIndex, FT_Face* face) {
    const HANDLE file = CreateFileW(std::filesystem::path(reinterpret_cast<const char8_t*>(path)).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return FT_Err_Cannot_Open_Resource;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > std::numeric_limits<long>::max()) {
        CloseHandle(file);
        return FT_Err_Cannot_Open_Stream;
    }
    const HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    CloseHandle(file);
    if (!mapping) return FT_Err_Cannot_Open_Stream;
    void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(mapping);
    if (!view) return FT_Err_Cannot_Open_Stream;
    auto* stream = new FT_StreamRec{};
    stream->base = static_cast<unsigned char*>(view);
    stream->size = static_cast<unsigned long>(size.QuadPart);
    stream->close = &CloseMappedFile;
    FT_Open_Args args{};
    args.flags = FT_OPEN_STREAM;
    args.stream = stream;
    return FT_Open_Face(library, &args, subFontIndex, face);
}
#else
FT_Error OpenFileFace(FT_Library library, const char* path, FT_Long subFontIndex, FT_Face* face) {
    return FT_New_Face(library, path, subFontIndex, face);
}
#endif

int APS5_VABI LibraryOpenFont(FontLibNative* library, std::uint32_t mode, const void* fontAddress, std::uint32_t fontSize, std::uint32_t subFontIndex, std::uint32_t uniqueWord, FontObj** inoutFontObj) {
    (void)uniqueWord;
    FontLibNative localLibrary{};
    FontObj* headObj = nullptr;
    if (!CopyInGuest(localLibrary, library) || !fontAddress || !CopyInGuest(headObj, inoutFontObj)) {
        return SCE_FONT_ERROR_INVALID_PARAMETER;
    }
    FontMemoryInterface localIface{};
    const bool hasIface = localLibrary.iface && CopyInGuest(localIface, localLibrary.iface);
    const auto allocFn = hasIface ? localIface.alloc : nullptr;
    const auto freeFn = hasIface ? localIface.dealloc : nullptr;
    if (!allocFn || !freeFn) return SCE_FONT_ERROR_INVALID_PARAMETER;
    const bool fromMemory = mode == 1;
    std::string fontPath;
    if (fromMemory) {
        if (fontSize == 0) return SCE_FONT_ERROR_INVALID_PARAMETER;
    } else if (mode == 5 || mode == 6 || mode == 7) {
        if (CopyPathFromGuest(static_cast<const char*>(fontAddress), fontPath) != 0 || fontPath.empty()) {
            return SCE_FONT_ERROR_INVALID_PARAMETER;
        }
    } else {
        return SCE_FONT_ERROR_INVALID_PARAMETER;
    }
    auto* ctx = static_cast<FtLibraryContext*>(localLibrary.fontset_registry);
    if (!ctx || !ctx->library) return SCE_FONT_ERROR_INVALID_LIBRARY;
    const bool systemSet = (localLibrary.sysfont_flags & SYSFONT_FLAG_SYSTEM_SET) != 0;
    FT_Library* ftLibrary = systemSet ? &ctx->hostLibrary : &ctx->library;
    if (systemSet && !SystemFontLibrary(ctx)) return SCE_FONT_ERROR_ALLOCATION_FAILED;
    FT_Face face = nullptr;
    FT_Error error;
    if (fromMemory) {
        error = FT_New_Memory_Face(*ftLibrary, static_cast<const FT_Byte*>(fontAddress), static_cast<FT_Long>(fontSize), static_cast<FT_Long>(subFontIndex), &face);
    } else {
        error = OpenFileFace(*ftLibrary, fontPath.c_str(), static_cast<FT_Long>(subFontIndex), &face);
    }
    if (error != 0 || !face) {
        if (fromMemory || error == FT_Err_Unknown_File_Format) return SCE_FONT_ERROR_NO_SUPPORT_FORMAT;
        return SCE_FONT_ERROR_FS_OPEN_FAILED;
    }
    FT_Select_Charmap(face, FT_ENCODING_UNICODE);
    auto* obj = static_cast<FontObj*>(allocFn(localLibrary.alloc_ctx, sizeof(FontObj)));
    if (!obj) {
        FT_Done_Face(face);
        return SCE_FONT_ERROR_ALLOCATION_FAILED;
    }
    FontObj localObj{};
    localObj.refcount = 1;
    localObj.sub_font_index = subFontIndex;
    localObj.prev = nullptr;
    localObj.next = headObj;
    localObj.ft_face = face;
    localObj.ft_ctx_0x58 = ftLibrary;
    if (headObj) {
        FontObj nextObj{};
        if (!CopyInGuest(nextObj, headObj)) {
            FT_Done_Face(face);
            freeFn(localLibrary.alloc_ctx, obj);
            return SCE_FONT_ERROR_INVALID_PARAMETER;
        }
        nextObj.prev = obj;
        if (!CopyOutGuest(headObj, nextObj)) {
            FT_Done_Face(face);
            freeFn(localLibrary.alloc_ctx, obj);
            return SCE_FONT_ERROR_INVALID_PARAMETER;
        }
    }
    if (!CopyOutGuest(obj, localObj) || !CopyOutGuest(inoutFontObj, obj)) {
        if (headObj) {
            FontObj nextObj{};
            if (CopyInGuest(nextObj, headObj)) {
                nextObj.prev = nullptr;
                CopyOutGuest(headObj, nextObj);
            }
        }
        FT_Done_Face(face);
        freeFn(localLibrary.alloc_ctx, obj);
        return SCE_FONT_ERROR_INVALID_PARAMETER;
    }
    return SCE_FONT_OK;
}

int APS5_VABI LibraryCloseFont(FontObj* obj, std::uint32_t flags) {
    (void)flags;
    FontObj localObj{};
    if (!CopyInGuest(localObj, obj)) return SCE_FONT_ERROR_FATAL;
    if (localObj.refcount > 1) {
        --localObj.refcount;
        if (!CopyOutGuest(obj, localObj)) return SCE_FONT_ERROR_FATAL;
        return SCE_FONT_OK;
    }
    FT_Face face = FaceOf(&localObj);
    FtLibraryContext* ctx = face && face->memory ? static_cast<FtLibraryContext*>(face->memory->user) : nullptr;
    const auto freeFn = ctx && ctx->iface ? ctx->iface->dealloc : nullptr;
    if (face) {
        FT_Done_Face(face);
        localObj.ft_face = nullptr;
    }
    if (!freeFn) {
        CopyOutGuest(obj, localObj);
        return SCE_FONT_ERROR_FATAL;
    }
    FontObj* next = localObj.next;
    FontObj* prev = localObj.prev;
    if (prev) {
        FontObj prevObj{};
        if (CopyInGuest(prevObj, prev)) {
            prevObj.next = next;
            CopyOutGuest(prev, prevObj);
        }
    }
    if (next) {
        FontObj nextObj{};
        if (CopyInGuest(nextObj, next)) {
            nextObj.prev = prev;
            CopyOutGuest(next, nextObj);
        }
    }
    freeFn(ctx->allocCtx, obj);
    return SCE_FONT_OK;
}

int APS5_VABI LibraryGetFaceScale(FontObj* obj, std::uint32_t* outUnitsPerEm, float* outScale) {
    FontObj localObj{};
    if (!CopyInGuest(localObj, obj) || !outUnitsPerEm || !outScale) return SCE_FONT_ERROR_FATAL;
    const FT_Face face = FaceOf(&localObj);
    if (!face) return SCE_FONT_ERROR_FATAL;
    const std::uint32_t units = face->units_per_EM;
    const float scale = static_cast<float>(units) * ONE_OVER_64;
    if (!CopyOutGuest(outUnitsPerEm, units) || !CopyOutGuest(outScale, scale)) return SCE_FONT_ERROR_FATAL;
    return SCE_FONT_OK;
}

int APS5_VABI LibraryGetFaceMetric(FontObj* obj, std::uint32_t metricId, std::uint16_t* outMetric) {
    FontObj localObj{};
    if (!CopyInGuest(localObj, obj) || !outMetric) return SCE_FONT_ERROR_FATAL;
    const FT_Face face = FaceOf(&localObj);
    if (!face) return SCE_FONT_ERROR_FATAL;
    const std::uint16_t units = face->units_per_EM;
    std::uint16_t metric = 0;
    int rc = SCE_FONT_OK;
    if (metricId == 0x0E00) {
        metric = units;
    } else if (metricId == 0xEA00) {
        if (const auto* os2 = static_cast<const TT_OS2*>(FT_Get_Sfnt_Table(face, FT_SFNT_OS2))) {
            metric = static_cast<std::uint16_t>(os2->sTypoAscender);
        } else {
            metric = units;
            rc = SCE_FONT_ERROR_NO_SUPPORT_FUNCTION;
        }
    }
    if (!CopyOutGuest(outMetric, metric)) return SCE_FONT_ERROR_FATAL;
    return rc;
}

int APS5_VABI LibraryGetGlyphsCount(FontObj* obj, std::uint32_t* outCount) {
    FontObj localObj{};
    if (!CopyInGuest(localObj, obj) || !outCount) return SCE_FONT_ERROR_FATAL;
    const FT_Face face = FaceOf(&localObj);
    if (!face || face->num_glyphs < 0) return SCE_FONT_ERROR_FATAL;
    const auto count = static_cast<std::uint32_t>(face->num_glyphs);
    if (!CopyOutGuest(outCount, count)) return SCE_FONT_ERROR_FATAL;
    return SCE_FONT_OK;
}

int APS5_VABI LibraryGetGlyphIndex(FontObj* obj, std::uint32_t codepoint, std::uint32_t* outGlyphIndex) {
    FontObj localObj{};
    if (!CopyInGuest(localObj, obj) || !outGlyphIndex) return SCE_FONT_ERROR_FATAL;
    const FT_Face face = FaceOf(&localObj);
    if (!face) {
        CopyOutGuest(outGlyphIndex, std::uint32_t{0});
        return SCE_FONT_ERROR_FATAL;
    }
    const std::uint32_t glyphIndex = ResolveGlyphIndexWithFallback(face, codepoint);
    if (!CopyOutGuest(outGlyphIndex, glyphIndex)) return SCE_FONT_ERROR_FATAL;
    return glyphIndex ? SCE_FONT_OK : SCE_FONT_ERROR_NO_SUPPORT_GLYPH;
}

int APS5_VABI LibrarySetCharSizeWithDpi(FontObj* obj, std::uint32_t dpiX, std::uint32_t dpiY, float scaleX, float scaleY, float* outScaleX, float* outScaleY) {
    FontObj localObj{};
    if (!CopyInGuest(localObj, obj) || !outScaleX || !outScaleY) return SCE_FONT_ERROR_FATAL;
    const FT_Face face = FaceOf(&localObj);
    if (!face) return SCE_FONT_ERROR_FATAL;
    const auto charW = static_cast<FT_F26Dot6>(static_cast<std::int32_t>(scaleX * 64.0f));
    const auto charH = static_cast<FT_F26Dot6>(static_cast<std::int32_t>(scaleY * 64.0f));
    if (SetCharSizeCompat(face, charW, charH, dpiX, dpiY) != 0) return SCE_FONT_ERROR_FATAL;
    if (!face->size) return SCE_FONT_ERROR_FATAL;
    const std::uint16_t units = face->units_per_EM;
    const float resultScaleX = FixedMulUnitsToF26Dot6(static_cast<std::int64_t>(face->size->metrics.x_scale), units);
    const float resultScaleY = FixedMulUnitsToF26Dot6(static_cast<std::int64_t>(face->size->metrics.y_scale), units);
    if (!CopyOutGuest(outScaleX, resultScaleX) || !CopyOutGuest(outScaleY, resultScaleY)) return SCE_FONT_ERROR_FATAL;
    return SCE_FONT_OK;
}

int APS5_VABI LibrarySetCharSizeDefaultDpi(FontObj* obj, float scaleX, float scaleY, float* outScaleX, float* outScaleY) {
    if (!obj || !outScaleX || !outScaleY) return SCE_FONT_ERROR_FATAL;
    return LibrarySetCharSizeWithDpi(obj, 0x48, 0x48, scaleX, scaleY, outScaleX, outScaleY);
}

int APS5_VABI LibraryComputeLayout(FontObj* obj, const StyleStateBlock* style, std::uint8_t* outWords) {
    FontObj localObj{};
    StyleStateBlock localStyle{};
    if (!CopyInGuest(localObj, obj) || !CopyInGuest(localStyle, style) || !outWords) return SCE_FONT_ERROR_FATAL;
    const FT_Face face = FaceOf(&localObj);
    if (!face || !face->size) return SCE_FONT_ERROR_FATAL;
    const float effectWidth = localStyle.effect_weight_x;
    const float effectHeight = localStyle.effect_weight_y;
    const float slant = localStyle.slant_ratio;
    const auto yScale = static_cast<std::int64_t>(face->size->metrics.y_scale);
    const auto xScale = static_cast<std::int64_t>(face->size->metrics.x_scale);
    const std::int64_t yShift = localObj.shift_cache_y;
    const std::int64_t xShift = localObj.shift_cache_x;
    const auto unitsPerEm = static_cast<std::int64_t>(static_cast<std::uint16_t>(face->units_per_EM));
    std::int32_t yMinPx = RoundMulFixed(static_cast<std::int64_t>(face->bbox.yMin) + yShift, yScale);
    std::int32_t yMaxPx = RoundMulFixed(static_cast<std::int64_t>(face->bbox.yMax) + yShift, yScale);
    std::int32_t halfEffectWidthPx = 0;
    std::int32_t leftAdjustPx = 0;
    if (effectWidth != 0.0f) {
        const std::int32_t unitsScaledX = TruncFixed(unitsPerEm * xScale);
        halfEffectWidthPx = TruncateFloatToInt(effectWidth * static_cast<float>(unitsScaledX)) / 2;
        leftAdjustPx = -halfEffectWidthPx;
    }
    float outEffectHeight = 0.0f;
    if (effectHeight != 0.0f) {
        const std::int32_t unitsScaledY = TruncFixed(unitsPerEm * yScale);
        const std::int32_t halfEffectHeightPx = TruncateFloatToInt(effectHeight * static_cast<float>(unitsScaledY)) / 2;
        outEffectHeight = static_cast<float>(halfEffectHeightPx) * ONE_OVER_64;
        yMinPx -= halfEffectHeightPx;
        yMaxPx += halfEffectHeightPx;
    }
    if (slant != 0.0f) {
        const auto shear = static_cast<std::int64_t>(TruncateFloatToInt(slant * 65536.0f));
        leftAdjustPx += RoundMulFixed(yMinPx, shear);
        halfEffectWidthPx += RoundMulFixed(yMaxPx, shear);
    }
    std::uint8_t localWords[HORIZONTAL_LEFT_ADJUST + sizeof(float)]{};
    StoreFloat(localWords, HORIZONTAL_EFFECT_HEIGHT, outEffectHeight);
    StoreFloat(localWords, HORIZONTAL_LEFT_ADJUST, static_cast<float>(leftAdjustPx) * ONE_OVER_64);
    StoreFloat(localWords, HORIZONTAL_HALF_EFFECT_WIDTH, static_cast<float>(halfEffectWidthPx) * ONE_OVER_64);
    const std::int32_t xMinPx = RoundMulFixed(static_cast<std::int64_t>(face->bbox.xMin) + xShift, xScale);
    const std::int32_t xMaxPx = RoundMulFixed(static_cast<std::int64_t>(face->bbox.xMax) + xShift, xScale);
    StoreFloat(localWords, HORIZONTAL_LINE_ADVANCE, static_cast<float>(yMaxPx - yMinPx) * ONE_OVER_64);
    StoreFloat(localWords, HORIZONTAL_BASELINE, static_cast<float>(yMaxPx) * ONE_OVER_64);
    StoreFloat(localWords, HORIZONTAL_X_BOUND_LO, static_cast<float>(xMinPx + leftAdjustPx) * ONE_OVER_64);
    StoreFloat(localWords, HORIZONTAL_X_BOUND_HI, static_cast<float>(xMaxPx + halfEffectWidthPx) * ONE_OVER_64);
    const std::int32_t maxAdvanceWidthPx = RoundMulFixed(static_cast<std::int64_t>(face->max_advance_width) + xShift, xScale);
    StoreFloat(localWords, HORIZONTAL_MAX_ADVANCE_WIDTH, static_cast<float>(maxAdvanceWidthPx) * ONE_OVER_64);
    float caretRiseAdjust = 0.0f;
    if (const auto* hhea = static_cast<const TT_HoriHeader*>(FT_Get_Sfnt_Table(face, FT_SFNT_HHEA))) {
        const std::int64_t caretRiseUnits = xShift + static_cast<std::int64_t>(hhea->caret_Slope_Rise);
        const std::int32_t caretRisePx = TruncFixed(caretRiseUnits * xScale);
        caretRiseAdjust = static_cast<float>(caretRisePx - halfEffectWidthPx) * ONE_OVER_64;
    }
    StoreFloat(localWords, HORIZONTAL_CARET_RISE_ADJUST, caretRiseAdjust);
    if (!CopyOutGuestBytes(outWords, localWords, sizeof(localWords))) return SCE_FONT_ERROR_FATAL;
    return SCE_FONT_OK;
}

int APS5_VABI LibraryComputeLayoutAlt(FontObj* obj, const StyleStateBlock* style, std::uint8_t* outWords) {
    FontObj localObj{};
    StyleStateBlock localStyle{};
    if (!CopyInGuest(localObj, obj) || !CopyInGuest(localStyle, style) || !outWords) return SCE_FONT_ERROR_FATAL;
    const FT_Face face = FaceOf(&localObj);
    if (!face || !face->size) return SCE_FONT_ERROR_FATAL;
    const float effectWidth = localStyle.effect_weight_x;
    const float effectHeight = localStyle.effect_weight_y;
    const float slant = localStyle.slant_ratio;
    const auto xScale = static_cast<std::int64_t>(face->size->metrics.x_scale);
    const auto yScale = static_cast<std::int64_t>(face->size->metrics.y_scale);
    const auto* vhea = static_cast<const TT_VertHeader*>(FT_Get_Sfnt_Table(face, FT_SFNT_VHEA));
    const std::int64_t yShift = localObj.shift_cache_y;
    const std::int64_t xShift = localObj.shift_cache_x;
    const auto units = static_cast<std::uint16_t>(face->units_per_EM);
    std::int64_t yAscender = 0;
    std::int64_t yDescender = 0;
    if (vhea) {
        yAscender = static_cast<std::int64_t>(vhea->Ascender) + yShift;
        yDescender = static_cast<std::int64_t>(vhea->Descender) + yShift;
    }
    const std::int32_t scaledLeft = RoundMulFixed(static_cast<std::int64_t>(face->bbox.xMin) + xShift, xScale);
    const std::int32_t scaledRight = RoundMulFixed(static_cast<std::int64_t>(face->bbox.xMax) + xShift, xScale);
    std::int32_t xMinScaled = scaledLeft;
    std::int32_t xAbsMax = -xMinScaled;
    if (xAbsMax < scaledRight) {
        xAbsMax = scaledRight;
        xMinScaled = -scaledRight;
    }
    float outEffectWidth = 0.0f;
    if (effectWidth != 0.0f) {
        const auto unitsScaled = static_cast<std::int32_t>(TruncMulUnits(xScale, units));
        const std::int32_t effect = TruncateFloatToInt(effectWidth * static_cast<float>(unitsScaled)) / 2;
        outEffectWidth = static_cast<float>(effect) * ONE_OVER_64;
    }
    if (effectHeight != 0.0f) {
        const auto unitsScaled = static_cast<std::int32_t>(TruncMulUnits(yScale, units));
        const std::int32_t delta = TruncateFloatToInt(effectHeight * static_cast<float>(unitsScaled)) / 2;
        yAscender -= delta;
        yDescender += delta;
    }
    float outSlantA = 0.0f;
    float outSlantB = 0.0f;
    if (slant != 0.0f) {
        const auto shear = static_cast<std::int64_t>(TruncateFloatToInt(slant * 65536.0f));
        const std::int64_t signAdjustMin = yAscender < 1 ? 0x10000LL : 0LL;
        std::int64_t roundedMin = signAdjustMin + (-yAscender) * shear - 0x8000LL;
        if (roundedMin < 0) roundedMin = signAdjustMin + 0x7FFFLL + (-yAscender) * shear;
        const std::int64_t signAdjustMax = yDescender >= 0 ? 0x10000LL : 0LL;
        std::int64_t roundedMax = signAdjustMax + yDescender * shear - 0x8000LL;
        if (roundedMax < 0) roundedMax = signAdjustMax + 0x7FFFLL + yDescender * shear;
        const auto roundedMaxBits = static_cast<std::uint64_t>(roundedMax);
        const auto roundedMinBits = static_cast<std::uint64_t>(roundedMin);
        const std::uint64_t roundedMaxHigh = roundedMaxBits >> 16;
        const bool maxRoundBitClear = ((roundedMaxBits >> 15) & 1u) == 0;
        std::uint64_t selectedHigh = roundedMaxHigh;
        auto selectedLow = static_cast<std::int32_t>(roundedMinBits >> 16);
        if (maxRoundBitClear && roundedMaxHigh != 0) {
            selectedHigh = roundedMinBits >> 16;
            selectedLow = static_cast<std::int32_t>(roundedMaxBits >> 16);
        }
        outSlantA = static_cast<float>(selectedLow) * ONE_OVER_64;
        outSlantB = static_cast<float>(static_cast<std::int32_t>(selectedHigh)) * ONE_OVER_64;
    }
    std::uint8_t localWords[VERTICAL_LAYOUT_SIZE]{};
    StoreFloat(localWords, VERTICAL_COLUMN_ADVANCE, static_cast<float>(xAbsMax - xMinScaled) * ONE_OVER_64);
    StoreFloat(localWords, VERTICAL_BASELINE_OFFSET_X, static_cast<float>(-xAbsMax) * ONE_OVER_64);
    StoreFloat(localWords, VERTICAL_METRICS_0X08, static_cast<float>(RoundMulFixed(yAscender, xScale)) * ONE_OVER_64);
    StoreFloat(localWords, VERTICAL_METRICS_0X0C, static_cast<float>(RoundMulFixed(yDescender, xScale)) * ONE_OVER_64);
    const std::int32_t advanceHeight = RoundMulFixed(static_cast<std::int64_t>(face->max_advance_height) + yShift, yScale);
    StoreFloat(localWords, VERTICAL_ADVANCE_HEIGHT, static_cast<float>(advanceHeight) * ONE_OVER_64);
    StoreFloat(localWords, VERTICAL_DECORATION_SPAN, outEffectWidth);
    StoreFloat(localWords, VERTICAL_DECORATION_0X08, outSlantB);
    StoreFloat(localWords, VERTICAL_DECORATION_0X0C, outSlantA);
    if (!CopyOutGuestBytes(outWords, localWords, sizeof(localWords))) return SCE_FONT_ERROR_FATAL;
    return SCE_FONT_OK;
}

void StoreSeedVector(FontObj* obj) {
    const auto seedLow = static_cast<std::int32_t>(static_cast<std::uint32_t>(obj->layout_seed_pair & 0xFFFFFFFFu));
    const auto seedHigh = static_cast<std::int32_t>(static_cast<std::uint32_t>(obj->layout_seed_pair >> 32));
    obj->layout_seed_vec[0] = static_cast<std::uint64_t>(static_cast<std::int64_t>(seedLow));
    obj->layout_seed_vec[1] = static_cast<std::uint64_t>(static_cast<std::int64_t>(seedHigh));
}

void WriteOutlineSizes(FT_GlyphSlot slot, std::uint64_t* outWords) {
    const auto contours = static_cast<std::uint32_t>(slot->outline.n_contours);
    const auto points = static_cast<std::uint32_t>(slot->outline.n_points);
    const std::uint32_t size4 = points * 0x10u + 0x68u + ((points + 0x0Fu + contours * 2u) & 0xFFFFFFF0u);
    const std::uint32_t size8 = ((points + 0x2Bu + contours * 2u) & 0xFFFFFFFCu) + 0x10u + points * 4u;
    auto* out = reinterpret_cast<std::uint8_t*>(outWords);
    WriteU16(out, 0, static_cast<std::uint16_t>(contours));
    WriteU16(out, 2, static_cast<std::uint16_t>(points));
    WriteU32(out, 4, size4);
    WriteU32(out, 8, size8);
}

int APS5_VABI LibraryLoadGlyphCached(FontObj* obj, std::uint32_t glyphIndex, std::int32_t mode, std::uint64_t* outWords) {
    std::array<std::uint64_t, 2> localOutWords{};
    if (outWords && !CopyOutGuestBytes(outWords, localOutWords.data(), sizeof(localOutWords))) {
        return SCE_FONT_ERROR_FATAL;
    }
    FontObj localObj{};
    if (!CopyInGuest(localObj, obj) || !outWords) return SCE_FONT_ERROR_FATAL;
    const FT_Face face = FaceOf(&localObj);
    if (!face || !face->glyph) return SCE_FONT_ERROR_FATAL;
    const auto unitsPerEm = static_cast<std::uint16_t>(face->units_per_EM);
    const bool cachedMatch = localObj.cached_glyph_index_0x64 == static_cast<std::int32_t>(glyphIndex) && localObj.cached_units_x_0x68 == unitsPerEm && localObj.cached_units_y_0x70 == localObj.cached_units_x_0x68 && mode == 0;
    if (cachedMatch) {
        localObj.shift_cache_x = localObj.shift_units_x;
        localObj.shift_cache_y = localObj.shift_units_y;
        StoreSeedVector(&localObj);
        WriteOutlineSizes(face->glyph, localOutWords.data());
        if (!CopyOutGuest(obj, localObj) || !CopyOutGuestBytes(outWords, localOutWords.data(), sizeof(localOutWords))) {
            return SCE_FONT_ERROR_FATAL;
        }
        return SCE_FONT_OK;
    }
    const FT_Int32 loadFlags = (mode == 0 ? FT_LOAD_NO_SCALE : 0) | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_VERTICAL_LAYOUT;
    const FT_Error error = FT_Load_Glyph(face, glyphIndex, loadFlags);
    if (error != 0) {
        localObj.cached_glyph_index_0x64 = 0;
        localObj.cached_units_x_0x68 = 0;
        localObj.cached_units_y_0x70 = 0;
        localObj.layout_seed_vec = {};
        localObj.layout_scale_vec = {};
        CopyOutGuest(obj, localObj);
        return error == FT_Err_Out_Of_Memory ? SCE_FONT_ERROR_ALLOCATION_FAILED : SCE_FONT_ERROR_NO_SUPPORT_GLYPH;
    }
    if (!face->glyph) return SCE_FONT_ERROR_FATAL;
    localObj.cached_glyph_index_0x64 = static_cast<std::int32_t>(glyphIndex);
    if (mode == 0) {
        localObj.cached_units_x_0x68 = unitsPerEm;
        localObj.cached_units_y_0x70 = unitsPerEm;
        localObj.shift_cache_x = localObj.shift_units_x;
        localObj.shift_cache_y = localObj.shift_units_y;
        StoreSeedVector(&localObj);
        localObj.layout_scale_vec[0] = 0x10000;
        localObj.layout_scale_vec[1] = 0x10000;
    } else {
        if (!face->size) return SCE_FONT_ERROR_FATAL;
        const auto xScale = static_cast<std::int64_t>(face->size->metrics.x_scale);
        const auto yScale = static_cast<std::int64_t>(face->size->metrics.y_scale);
        localObj.cached_units_x_0x68 = static_cast<std::uint64_t>(TruncMulUnits(xScale, unitsPerEm));
        localObj.cached_units_y_0x70 = static_cast<std::uint64_t>(TruncMulUnits(yScale, unitsPerEm));
        localObj.shift_cache_x = RoundMulFixed(localObj.shift_units_x, xScale);
        localObj.shift_cache_y = RoundMulFixed(localObj.shift_units_y, yScale);
        const auto seedLow = static_cast<std::int32_t>(static_cast<std::uint32_t>(localObj.layout_seed_pair & 0xFFFFFFFFu));
        const auto seedHigh = static_cast<std::int32_t>(static_cast<std::uint32_t>(localObj.layout_seed_pair >> 32));
        localObj.layout_seed_vec[0] = static_cast<std::uint64_t>(static_cast<std::int64_t>(RoundMulFixed(seedLow, xScale)));
        localObj.layout_seed_vec[1] = static_cast<std::uint64_t>(static_cast<std::int64_t>(RoundMulFixed(seedHigh, yScale)));
        localObj.layout_scale_vec[0] = static_cast<std::uint64_t>(xScale);
        localObj.layout_scale_vec[1] = static_cast<std::uint64_t>(yScale);
    }
    WriteOutlineSizes(face->glyph, localOutWords.data());
    if (!CopyOutGuest(obj, localObj) || !CopyOutGuestBytes(outWords, localOutWords.data(), sizeof(localOutWords))) {
        return SCE_FONT_ERROR_FATAL;
    }
    return SCE_FONT_OK;
}

std::int64_t RoundMulFixed64(std::int64_t value, std::int64_t fixed16) {
    return static_cast<std::int64_t>(RoundMulFixed(value, fixed16));
}

int APS5_VABI LibraryGetGlyphMetrics(FontObj* obj, std::uint32_t* optParam2, std::uint8_t mode, std::uint8_t* outParams, FontGlyphMetrics* outMetrics) {
    FontObj localObj{};
    if (!CopyInGuest(localObj, obj) || !outParams || !outMetrics) return SCE_FONT_ERROR_FATAL;
    const FT_Face face = FaceOf(&localObj);
    if (!face || !face->glyph || !face->size) return SCE_FONT_ERROR_FATAL;
    const FT_GlyphSlot slot = face->glyph;
    std::uint8_t localParams[0x48]{};
    FontGlyphMetrics localMetrics{};
    const void* outlineBlob = &slot->outline;
    localParams[0] = 0xF2;
    WritePointer(localParams, 0x08, slot);
    if (optParam2) {
        outlineBlob = optParam2;
        localParams[0] = 0xF0;
        WritePointer(localParams, 0x08, optParam2);
    }
    const auto faceFlagBit = static_cast<std::uint8_t>((static_cast<std::uint32_t>(face->face_flags) >> 5) & 1u);
    const bool hasSeedVector = localObj.layout_seed_vec[0] != 0 || localObj.layout_seed_vec[1] != 0;
    localParams[2] = static_cast<std::uint8_t>(0xF0u | (hasSeedVector ? 0x08u : 0u) | faceFlagBit);
    localParams[3] = 0;
    WriteU16(localParams, 0x04, static_cast<std::uint16_t>(face->units_per_EM));
    WritePointer(localParams, 0x10, outlineBlob);
    WritePointer(localParams, 0x18, outMetrics);
    float styleShiftX = 0.0f;
    float styleShiftY = 0.0f;
    if (localObj.reserved_0x04 != 0) {
        styleShiftX = static_cast<float>(slot->metrics.vertBearingX - slot->metrics.horiBearingX);
        styleShiftY = static_cast<float>(-(slot->metrics.horiBearingY + slot->metrics.vertBearingY));
        localParams[1] = 1;
    } else {
        localParams[1] = 0;
    }
    StoreFloat(localParams, 0x20, styleShiftX);
    StoreFloat(localParams, 0x24, styleShiftY);
    StoreFloat(localParams, 0x28, 1.0f);
    const auto shiftX = static_cast<std::int32_t>(localObj.shift_cache_x);
    const auto shiftY = static_cast<std::int32_t>(localObj.shift_cache_y);
    const auto seedX = static_cast<std::int32_t>(localObj.layout_seed_vec[0]);
    const auto seedY = static_cast<std::int32_t>(localObj.layout_seed_vec[1]);
    WriteS32(localParams, 0x2C, shiftX);
    WriteS32(localParams, 0x30, shiftY);
    WriteS32(localParams, 0x34, seedX);
    WriteS32(localParams, 0x38, seedY);
    StoreFloat(localParams, 0x3C, static_cast<float>(static_cast<std::int64_t>(localObj.cached_units_x_0x68)) * ONE_OVER_64);
    StoreFloat(localParams, 0x40, static_cast<float>(static_cast<std::int64_t>(localObj.cached_units_y_0x70)) * ONE_OVER_64);
    WriteU32(localParams, 0x44, 0);
    auto xScale = static_cast<std::int64_t>(face->size->metrics.x_scale);
    auto yScale = static_cast<std::int64_t>(face->size->metrics.y_scale);
    const auto objScaleX = static_cast<std::int64_t>(localObj.layout_scale_vec[0]);
    const auto objScaleY = static_cast<std::int64_t>(localObj.layout_scale_vec[1]);
    if (objScaleX != 0 && objScaleX != 0x10000) xScale = (xScale << 16) / objScaleX;
    if (objScaleY != 0 && objScaleY != 0x10000) yScale = (yScale << 16) / objScaleY;
    const bool applyScale = (mode & 0x0Fu) != 0;
    const auto scaleIf = [&](std::int64_t value, std::int64_t scale) {
        return applyScale ? RoundMulFixed64(value, scale) : value;
    };
    const auto widthRaw = static_cast<std::int64_t>(slot->metrics.width);
    const auto heightRaw = static_cast<std::int64_t>(slot->metrics.height);
    const auto horiBearingXRaw = static_cast<std::int64_t>(slot->metrics.horiBearingX);
    const auto horiAdvanceRaw = static_cast<std::int64_t>(slot->metrics.horiAdvance);
    const auto vertBearingXRaw = static_cast<std::int64_t>(slot->metrics.vertBearingX);
    const auto vertBearingYRaw = static_cast<std::int64_t>(slot->metrics.vertBearingY);
    const auto vertAdvanceRaw = static_cast<std::int64_t>(slot->metrics.vertAdvance);
    const std::int64_t horiAdvanceUnscaled = seedX + horiAdvanceRaw;
    const std::int64_t vertAdvanceUnscaled = seedY + vertAdvanceRaw;
    slot->metrics.horiAdvance = static_cast<FT_Pos>(horiAdvanceUnscaled);
    slot->metrics.vertAdvance = static_cast<FT_Pos>(vertAdvanceUnscaled);
    if (widthRaw == 0 && heightRaw == 0) {
        localMetrics.Horizontal.advance = FromF26Dot6(scaleIf(horiAdvanceUnscaled, xScale));
        localMetrics.Vertical.advance = FromF26Dot6(scaleIf(vertAdvanceUnscaled, yScale));
        if (!CopyOutGuestBytes(outParams, localParams, sizeof(localParams)) || !CopyOutGuest(outMetrics, localMetrics)) {
            return SCE_FONT_ERROR_FATAL;
        }
        return SCE_FONT_OK;
    }
    const std::int64_t topShift = heightRaw == 0 ? 0 : shiftY;
    std::int64_t left = horiBearingXRaw + shiftX;
    std::int64_t top = vertBearingYRaw + topShift;
    std::int64_t right = left + widthRaw;
    std::int64_t bottom = top - heightRaw;
    left = scaleIf(left, xScale);
    top = scaleIf(top, yScale);
    right = scaleIf(right, xScale);
    bottom = scaleIf(bottom, yScale);
    const std::int64_t horiAdvance = scaleIf(horiAdvanceUnscaled, xScale);
    const std::int64_t vertAdvance = scaleIf(vertAdvanceUnscaled, yScale);
    const std::int64_t outWidth = right - left;
    const std::int64_t outHeight = top - bottom;
    std::int64_t vertBearingX;
    std::int64_t vertBearingY;
    if ((static_cast<std::uint32_t>(face->face_flags) & 0x20u) == 0) {
        vertBearingX = left - horiAdvance / 2;
        vertBearingY = (vertAdvance - outHeight) / 2;
    } else {
        if (seedX == 0) {
            vertBearingX = scaleIf(shiftX + vertBearingXRaw, xScale);
        } else {
            slot->metrics.vertBearingX = static_cast<FT_Pos>(horiBearingXRaw - horiAdvanceUnscaled / 2);
            vertBearingX = left - horiAdvance / 2;
            StoreFloat(localParams, 0x20, static_cast<float>(-(horiAdvanceUnscaled / 2)));
        }
        vertBearingY = scaleIf(topShift + vertBearingYRaw, yScale);
    }
    localMetrics.width = FromF26Dot6(outWidth);
    localMetrics.height = FromF26Dot6(outHeight);
    localMetrics.Horizontal.bearingX = FromF26Dot6(left);
    localMetrics.Horizontal.bearingY = FromF26Dot6(top);
    localMetrics.Horizontal.advance = FromF26Dot6(horiAdvance);
    localMetrics.Vertical.bearingX = FromF26Dot6(vertBearingX);
    localMetrics.Vertical.bearingY = FromF26Dot6(vertBearingY);
    localMetrics.Vertical.advance = FromF26Dot6(vertAdvance);
    if ((mode & 0x06u) != 0 && localObj.font_handle) {
        FontHandleNative localHandle{};
        if (!CopyInGuest(localHandle, reinterpret_cast<const FontHandleNative*>(localObj.font_handle))) {
            return SCE_FONT_ERROR_FATAL;
        }
        const StyleStateBlock* style = (mode & 0x04u) ? &localHandle.cached_style.state : &localHandle.style;
        const float slantRatio = style->slant_ratio;
        if ((slantRatio != 0.0f || std::isnan(slantRatio)) && slot->outline.points && slot->outline.n_points > 0) {
            const auto shear = static_cast<std::int64_t>(TruncateFloatToInt(slantRatio * 65536.0f));
            std::int64_t minX = std::numeric_limits<std::int64_t>::max();
            std::int64_t maxX = std::numeric_limits<std::int64_t>::min();
            for (int i = 0; i < slot->outline.n_points; ++i) {
                const FT_Vector point = slot->outline.points[i];
                const std::int64_t px = scaleIf(static_cast<std::int64_t>(point.x) + shiftX, xScale);
                const std::int64_t py = scaleIf(static_cast<std::int64_t>(point.y) + shiftY, yScale);
                const std::int64_t slantedX = px + RoundMulFixed64(py, shear);
                minX = std::min(minX, slantedX);
                maxX = std::max(maxX, slantedX);
            }
            if (minX <= maxX) {
                localMetrics.width = FromF26Dot6(maxX - minX);
                localMetrics.Horizontal.bearingX = FromF26Dot6(minX);
            }
        }
        const float effectX = style->effect_weight_x;
        const float effectY = style->effect_weight_y;
        if (effectX != 0.0f || std::isnan(effectX) || effectY != 0.0f || std::isnan(effectY)) {
            const float dx = effectX * localObj.scale_x_0x50 * 0.5f;
            const float dy = effectY * localObj.scale_y_0x54 * 0.5f;
            localMetrics.width = localMetrics.width + dx + dx;
            localMetrics.height = localMetrics.height + dy + dy;
            localMetrics.Horizontal.bearingX -= dx;
            localMetrics.Horizontal.bearingY += dy;
            localMetrics.Vertical.bearingX -= dx;
            localMetrics.Vertical.bearingY -= dy;
            if (localMetrics.Horizontal.advance != 0.0f || std::isnan(localMetrics.Horizontal.advance)) localMetrics.Horizontal.advance += dx;
            if (localMetrics.Vertical.advance != 0.0f || std::isnan(localMetrics.Vertical.advance)) localMetrics.Vertical.advance += dy;
        }
    }
    if (!CopyOutGuestBytes(outParams, localParams, sizeof(localParams)) || !CopyOutGuest(outMetrics, localMetrics)) {
        return SCE_FONT_ERROR_FATAL;
    }
    return SCE_FONT_OK;
}

int APS5_VABI LibraryApplyGlyphAdjust(FontObj* obj, std::uint32_t p2, std::uint32_t glyphIndex, std::int32_t p4, std::int32_t p5, std::uint32_t* inoutGlyphIndex) {
    (void)p2;
    FontObj localObj{};
    if (!CopyInGuest(localObj, obj) || !inoutGlyphIndex) return SCE_FONT_ERROR_FATAL;
    localObj.shift_units_x = p4;
    localObj.shift_units_y = p5;
    localObj.layout_seed_pair = 0;
    if (!CopyOutGuest(inoutGlyphIndex, glyphIndex) || !CopyOutGuest(obj, localObj)) return SCE_FONT_ERROR_FATAL;
    return SCE_FONT_OK;
}

int APS5_VABI LibraryConfigureGlyph(FontObj* obj, std::uint32_t* inParams, std::int32_t mode, std::uint32_t* inoutState) {
    FontObj localObj{};
    std::uint32_t param0 = 0;
    std::uint32_t state = 0;
    if (!CopyInGuest(localObj, obj) || !CopyInGuest(param0, inParams) || !CopyInGuest(state, inoutState)) return SCE_FONT_ERROR_FATAL;
    if (param0 != 0) {
        state |= 0x80000000u;
        if (!CopyOutGuest(inoutState, state)) return SCE_FONT_ERROR_FATAL;
    }
    localObj.glyph_cfg_word_0x130 = param0;
    localObj.glyph_cfg_mode_0x134 = static_cast<std::uint8_t>(mode);
    localObj.glyph_cfg_byte_0x136 = 0;
    localObj.glyph_cfg_byte_0x135 = 0;
    if (!CopyOutGuest(obj, localObj)) return SCE_FONT_ERROR_FATAL;
    return SCE_FONT_OK;
}

int APS5_VABI RendererCreate(RendererNative* renderer) {
    if (!renderer) return SCE_FONT_ERROR_INVALID_RENDERER;
    auto* ft = reinterpret_cast<RendererFt*>(renderer);
    RendererFt localRenderer{};
    if (!CopyInGuest(localRenderer, ft)) return SCE_FONT_ERROR_INVALID_RENDERER;
    localRenderer.ft_backend.renderer_header_0x10 = &ft->base.mem_kind;
    localRenderer.ft_backend.unknown_0x08 = 0;
    localRenderer.ft_backend.unknown_0x10 = 0;
    localRenderer.ft_backend.unknown_0x18 = 0;
    localRenderer.ft_backend.initialized_marker = ft;
    if (!CopyOutGuest(ft, localRenderer)) return SCE_FONT_ERROR_INVALID_RENDERER;
    return SCE_FONT_OK;
}

int APS5_VABI RendererDestroy(RendererNative* renderer) {
    if (!renderer) return SCE_FONT_ERROR_INVALID_RENDERER;
    auto* ft = reinterpret_cast<RendererFt*>(renderer);
    RendererFt localRenderer{};
    if (!CopyInGuest(localRenderer, ft)) return SCE_FONT_ERROR_INVALID_RENDERER;
    localRenderer.ft_backend.initialized_marker = nullptr;
    if (!CopyOutGuest(ft, localRenderer)) return SCE_FONT_ERROR_INVALID_RENDERER;
    return SCE_FONT_OK;
}

std::uint64_t APS5_VABI RendererQuery(RendererNative* renderer, std::uint8_t* params, std::int64_t* outPtr, std::uint8_t* outVector) {
    (void)renderer;
    (void)params;
    if (outVector) {
        const std::uint8_t zero[16]{};
        CopyOutGuestBytes(outVector, zero, sizeof(zero));
    }
    if (outPtr) {
        const std::int64_t zero = 0;
        CopyOutGuest(outPtr, zero);
    }
    return 0;
}

float ScaledUnits(std::int64_t units, std::int64_t fixed16) {
    return FromF26Dot6(RoundMulFixed(units, fixed16));
}

void StoreVerticalFontMetrics(FT_Face face, const TT_OS2* os2, std::int64_t xShift, std::int64_t yShift, FontMetrics* metrics) {
    const auto xScale = static_cast<std::int64_t>(face->size->metrics.x_scale);
    const auto yScale = static_cast<std::int64_t>(face->size->metrics.y_scale);
    const auto unitsPerEm = static_cast<std::int64_t>(static_cast<std::uint16_t>(face->units_per_EM));
    const auto* vhea = static_cast<const TT_VertHeader*>(FT_Get_Sfnt_Table(face, FT_SFNT_VHEA));
    if (vhea) {
        const std::int64_t minTop = vhea->min_Top_Side_Bearing;
        metrics->ascender = ScaledUnits(minTop + yShift, yScale);
        metrics->descender = ScaledUnits(static_cast<std::int64_t>(vhea->yMax_Extent) + yShift, yScale);
        metrics->lineGap = 0.0f;
        if (vhea->Version >= 0x110000 && vhea->Line_Gap != 0) {
            metrics->lineGap = FromF26Dot6(RoundMulFixed(vhea->Line_Gap, xScale) - TruncFixed(unitsPerEm * xScale));
        }
        metrics->verticalDescender = ScaledUnits(static_cast<std::int64_t>(vhea->Descender) + xShift, xScale);
        const std::int64_t ascender = os2 ? os2->sTypoAscender : static_cast<std::int32_t>(minTop + face->bbox.yMax);
        metrics->verticalAscender = ScaledUnits(ascender + yShift, yScale);
    } else {
        std::int64_t ascender = 0;
        if (os2) {
            ascender = os2->sTypoAscender;
            metrics->ascender = ScaledUnits(ascender - face->bbox.yMax, yScale);
            metrics->descender = ScaledUnits(ascender - face->bbox.yMin, yScale);
            metrics->lineGap = 0.0f;
        } else {
            const std::int32_t lineGap = face->height - face->ascender + face->descender;
            const std::int32_t halfGap = lineGap / 2;
            metrics->ascender = ScaledUnits(halfGap, yScale);
            metrics->descender = ScaledUnits(face->ascender - face->descender + halfGap, yScale);
            metrics->lineGap = ScaledUnits(lineGap, xScale);
            ascender = face->ascender + halfGap;
        }
        metrics->verticalAscender = ScaledUnits(ascender + yShift, yScale);
        metrics->verticalDescender = ScaledUnits(-static_cast<std::int64_t>(unitsPerEm / 2), xScale);
    }
    metrics->maxAdvance = ScaledUnits(face->max_advance_height, yScale);
}

std::uint32_t StoreHorizontalFontMetrics(FT_Face face, const TT_OS2* os2, std::uint32_t form, std::int64_t xShift, std::int64_t yShift, FontMetrics* metrics) {
    const auto xScale = static_cast<std::int64_t>(face->size->metrics.x_scale);
    const auto yScale = static_cast<std::int64_t>(face->size->metrics.y_scale);
    const auto* hhea = static_cast<const TT_HoriHeader*>(FT_Get_Sfnt_Table(face, FT_SFNT_HHEA));
    metrics->verticalDescender = 0.0f;
    metrics->verticalAscender = 0.0f;
    bool fromHorizontalHeader = form == 1 || ((form == 2 || form == 3) && !os2);
    if (form == 2 && os2) {
        metrics->ascender = ScaledUnits(static_cast<std::int64_t>(os2->sTypoAscender) + yShift, yScale);
        metrics->descender = ScaledUnits(static_cast<std::int64_t>(os2->sTypoDescender) + yShift, yScale);
        metrics->lineGap = ScaledUnits(os2->sTypoLineGap, yScale);
        fromHorizontalHeader = metrics->ascender == 0.0f && metrics->descender == 0.0f;
    } else if (form == 3 && os2) {
        metrics->ascender = ScaledUnits(static_cast<std::int64_t>(os2->usWinAscent) + yShift, yScale);
        metrics->descender = FromF26Dot6(-RoundMulFixed(static_cast<std::int64_t>(os2->usWinDescent) - yShift, yScale));
        metrics->lineGap = hhea ? ScaledUnits(hhea->Line_Gap, yScale) : 0.0f;
        fromHorizontalHeader = metrics->ascender == metrics->descender;
    }
    if (fromHorizontalHeader) {
        form = 1;
        if (hhea) {
            metrics->ascender = ScaledUnits(static_cast<std::int64_t>(hhea->Ascender) + yShift, yScale);
            metrics->descender = ScaledUnits(static_cast<std::int64_t>(hhea->Descender) + yShift, yScale);
            metrics->lineGap = ScaledUnits(hhea->Line_Gap, yScale);
        } else {
            metrics->ascender = ScaledUnits(static_cast<std::int64_t>(face->ascender) + yShift, yScale);
            metrics->descender = ScaledUnits(static_cast<std::int64_t>(face->descender) + yShift, yScale);
            metrics->lineGap = 0.0f;
        }
    }
    metrics->maxAdvance = ScaledUnits(static_cast<std::int64_t>(face->max_advance_width) + xShift, xScale);
    return form;
}

int APS5_VABI LibraryGetFontMetrics(FontObj* obj, std::uint32_t form, FontMetrics* metrics) {
    FontObj localObj{};
    const FT_Face face = CopyInGuest(localObj, obj) ? FaceOf(&localObj) : nullptr;
    if (!face || !face->size || !metrics) return SCE_FONT_ERROR_FATAL;
    const auto xScale = static_cast<std::int64_t>(face->size->metrics.x_scale);
    const auto yScale = static_cast<std::int64_t>(face->size->metrics.y_scale);
    const std::int64_t xShift = localObj.shift_cache_x;
    const std::int64_t yShift = localObj.shift_cache_y;
    const auto* os2 = static_cast<const TT_OS2*>(FT_Get_Sfnt_Table(face, FT_SFNT_OS2));
    if (os2 && os2->version == 0xFFFF) os2 = nullptr;
    FontMetrics localMetrics{};
    if (form == 4) {
        StoreVerticalFontMetrics(face, os2, xShift, yShift, &localMetrics);
    } else {
        form = StoreHorizontalFontMetrics(face, os2, form, xShift, yShift, &localMetrics);
    }
    localMetrics.xMin = ScaledUnits(static_cast<std::int64_t>(face->bbox.xMin) + xShift, xScale);
    localMetrics.yMin = ScaledUnits(static_cast<std::int64_t>(face->bbox.yMin) + yShift, yScale);
    localMetrics.xMax = ScaledUnits(static_cast<std::int64_t>(face->bbox.xMax) + xShift, xScale);
    localMetrics.yMax = ScaledUnits(static_cast<std::int64_t>(face->bbox.yMax) + yShift, yScale);
    const auto unitsPerEm = static_cast<std::int64_t>(static_cast<std::uint16_t>(face->units_per_EM));
    localMetrics.emWidth = FromF26Dot6(TruncFixed(unitsPerEm * xScale));
    localMetrics.emHeight = FromF26Dot6(TruncFixed(unitsPerEm * yScale));
    FontHandleNative localHandle{};
    const bool hasHandle = localObj.font_handle && CopyInGuest(localHandle, reinterpret_cast<const FontHandleNative*>(localObj.font_handle));
    localMetrics.effectWeightX = hasHandle ? localHandle.style.effect_weight_x : 0.0f;
    localMetrics.effectWeightY = hasHandle ? localHandle.style.effect_weight_y : 0.0f;
    localMetrics.slantRatio = hasHandle ? localHandle.style.slant_ratio : 0.0f;
    localMetrics.form = form;
    if (!CopyOutGuest(metrics, localMetrics)) return SCE_FONT_ERROR_FATAL;
    return SCE_FONT_OK;
}

int APS5_VABI LibraryGetOutlineProfile(FontObj* obj, FontOutlineProfile* profile) {
    FontObj localObj{};
    const FT_Face face = CopyInGuest(localObj, obj) ? FaceOf(&localObj) : nullptr;
    if (!face) return SCE_FONT_ERROR_INVALID_FONT_HANDLE;
    if (!profile) return SCE_FONT_ERROR_INVALID_PARAMETER;
    const auto* maxp = static_cast<const TT_MaxProfile*>(FT_Get_Sfnt_Table(face, FT_SFNT_MAXP));
    if (!maxp) throw std::runtime_error("FontFt: outline profile of a face without a maxp table");
    if (maxp->version < 0x10000) return SCE_FONT_ERROR_NO_SUPPORT_FUNCTION;
    FontOutlineProfile localProfile{};
    localProfile.maxContours = maxp->maxContours;
    localProfile.maxPoints = maxp->maxPoints;
    localProfile.maxCompositeContours = maxp->maxCompositeContours;
    localProfile.maxCompositePoints = maxp->maxCompositePoints;
    if (!CopyOutGuest(profile, localProfile)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    return SCE_FONT_OK;
}

std::uint16_t WeightClass(std::uint16_t weight) {
    return weight >= 100 && weight <= 900 && weight % 100 == 0 ? static_cast<std::uint16_t>(weight / 100) : 0;
}

int APS5_VABI LibraryGetFontStyleInformation(FontObj* obj, FontStyleInformation* information) {
    FontObj localObj{};
    const FT_Face face = CopyInGuest(localObj, obj) ? FaceOf(&localObj) : nullptr;
    if (!face || !information) return SCE_FONT_ERROR_FATAL;
    const auto* header = static_cast<const TT_Header*>(FT_Get_Sfnt_Table(face, FT_SFNT_HEAD));
    if (!header) throw std::runtime_error("FontFt: style information of a face without a head table");
    const auto* os2 = static_cast<const TT_OS2*>(FT_Get_Sfnt_Table(face, FT_SFNT_OS2));
    if (os2 && os2->version == 0xFFFF) os2 = nullptr;
    FontStyleInformation localInformation{};
    localInformation.familyName = face->family_name;
    localInformation.styleName = face->style_name;
    localInformation.styleFlags = static_cast<std::uint16_t>(face->style_flags & (FT_STYLE_FLAG_ITALIC | FT_STYLE_FLAG_BOLD));
    localInformation.weight = os2 ? WeightClass(os2->usWeightClass) : 0;
    localInformation.width = os2 && os2->usWidthClass < 10 ? os2->usWidthClass : 0;
    localInformation.layoutFlags = static_cast<std::uint16_t>(1 + (header->Font_Direction < 0 ? 1 : 0) + (FT_HAS_VERTICAL(face) ? 0x80 : 0));
    if (!CopyOutGuest(information, localInformation)) return SCE_FONT_ERROR_FATAL;
    return SCE_FONT_OK;
}

SysDriver MakeDriverTable() {
    SysDriver driver{};
    driver.pixel_resolution = &LibraryGetPixelResolution;
    driver.init = &LibraryInit;
    driver.term = &LibraryTerm;
    driver.support_formats = &LibrarySupport;
    driver.open = &LibraryOpenFont;
    driver.close = &LibraryCloseFont;
    driver.scale = &LibraryGetFaceScale;
    driver.metric = &LibraryGetFaceMetric;
    driver.glyphs_count = &LibraryGetGlyphsCount;
    driver.glyph_index = &LibraryGetGlyphIndex;
    driver.set_char_with_dpi = &LibrarySetCharSizeWithDpi;
    driver.set_char_default_dpi = &LibrarySetCharSizeDefaultDpi;
    driver.font_metrics = &LibraryGetFontMetrics;
    driver.style_information = &LibraryGetFontStyleInformation;
    driver.outline_profile = &LibraryGetOutlineProfile;
    driver.compute_layout = &LibraryComputeLayout;
    driver.compute_layout_alt = &LibraryComputeLayoutAlt;
    driver.load_glyph_cached = &LibraryLoadGlyphCached;
    driver.get_glyph_metrics = &LibraryGetGlyphMetrics;
    driver.apply_glyph_adjust = &LibraryApplyGlyphAdjust;
    driver.configure_glyph = &LibraryConfigureGlyph;
    return driver;
}

const SysDriver driverTable = MakeDriverTable();
const RendererSelection rendererTable{0, sizeof(RendererFt), &RendererCreate, &RendererDestroy, &RendererQuery};

}

const Font::SysDriver* FontFt::DriverTable() {
    return &driverTable;
}

const Font::RendererSelection* FontFt::RendererTable() {
    return &rendererTable;
}

template <typename F>
static int WithLockedLibrary(FontLibrary library, F&& action) {
    auto* native = static_cast<Font::FontLibNative*>(library);
    if (!native) return SCE_FONT_ERROR_INVALID_LIBRARY;
    auto lockWord = std::atomic_ref<std::uint32_t>(native->lock_word);
    std::uint32_t previous = lockWord.load(std::memory_order_acquire);
    for (;;) {
        if (native->magic != Font::LIBRARY_MAGIC) return SCE_FONT_ERROR_INVALID_LIBRARY;
        if ((previous & Font::LOCK_BIT) == 0 && lockWord.compare_exchange_weak(previous, previous | Font::LOCK_BIT, std::memory_order_acq_rel)) break;
#ifdef _WIN32
        PreciseSleepUs(30);
#else
        std::this_thread::sleep_for(std::chrono::microseconds(30));
#endif
        previous = lockWord.load(std::memory_order_acquire);
    }
    const auto unlock = [previous](Font::FontLibNative* locked) {
        std::atomic_ref<std::uint32_t>(locked->lock_word).store(previous, std::memory_order_release);
    };
    const std::unique_ptr<Font::FontLibNative, decltype(unlock)> guard(native, unlock);
    if (native->sys_driver != &driverTable) throw std::runtime_error("FontFt: unsupported library driver");
    const auto* context = static_cast<const FtLibraryContext*>(native->fontset_registry);
    if (!context || !context->library) return SCE_FONT_ERROR_INVALID_LIBRARY;
    return action(context);
}

int FontFt::SupportFormats(FontLibrary library, std::initializer_list<const char*> modules) {
    return WithLockedLibrary(library, [&](const FtLibraryContext* context) {
        for (const char* module : modules) {
            if (!FT_Get_Module(context->library, module)) return SCE_FONT_ERROR_NO_SUPPORT_FORMAT;
        }
        return SCE_FONT_OK;
    });
}

int FontFt::SupportFontFormats(FontLibrary library, std::uint32_t formats) {
    return WithLockedLibrary(library, [&](const FtLibraryContext* context) {
        if ((formats & ~0xFFu) != 0) return SCE_FONT_ERROR_NO_SUPPORT_FORMAT;
        if ((formats & 0xC3u) != 0 && !FT_Get_Module(context->library, "truetype")) return SCE_FONT_ERROR_NO_SUPPORT_FORMAT;
        if ((formats & 0x50u) != 0 && !FT_Get_Module(context->library, "cff")) return SCE_FONT_ERROR_NO_SUPPORT_FORMAT;
        if ((formats & 0x20u) != 0 && !FT_Get_Module(context->library, "type1")) return SCE_FONT_ERROR_NO_SUPPORT_FORMAT;
        if ((formats & 0x04u) != 0 && !FT_Get_Module(context->library, "type42")) return SCE_FONT_ERROR_NO_SUPPORT_FORMAT;
        if ((formats & 0x08u) != 0 && !FT_Get_Module(context->library, "t1cid")) return SCE_FONT_ERROR_NO_SUPPORT_FORMAT;
        return SCE_FONT_OK;
    });
}
