// Derived from FreeType src/base/ftgloadr.c (FreeType 2.4.12, commit cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9).
// Copyright 2002-2006, 2010, 2013 by David Turner, Robert Wilhelm, and Werner Lemberg.
// This file is part of the FreeType project, and may only be used, modified, and distributed
// under the terms of the FreeType project license, LICENSE.TXT. By continuing to use, modify,
// or distribute this file you indicate that you have read the license and understand and accept it fully.

#include "prx/libSceFreeTypeOt/include/GlyphLoader.hpp"

#include "prx/libSceFreeTypeOt/include/Memory.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdint>
#include <cstring>

using namespace FreeType;

namespace {

constexpr std::int32_t ArrayTooLarge = 0x0A;
constexpr std::uint32_t OutlinePointsMax = 0x7FFF;
constexpr std::uint32_t OutlineContoursMax = 0x7FFF;
constexpr std::int64_t SubGlyphsMax = 0x7FFFFFFF / static_cast<std::int64_t>(sizeof(GuestSubGlyphRec));

std::int64_t Count(std::uint16_t value) {
    return static_cast<std::int16_t>(value);
}

std::int64_t PadCeil(std::int64_t value, std::int64_t step) {
    return (value + step - 1) & ~(step - 1);
}

std::int64_t Required(std::int64_t used, std::uint32_t requested) {
    return used + static_cast<std::int64_t>(requested);
}

GuestGlyphLoaderRec* Require(GuestGlyphLoaderRec* loader, const char* function) {
    if (!loader) NotImplemented_nid_no_patch(function);
    return loader;
}

template <typename T>
void Free(GuestMemoryRec* memory, T*& block) {
    ft_mem_free_nid_postfix(memory, block);
    block = nullptr;
}

template <typename T>
T* Allocate(GuestMemoryRec* memory, std::int64_t count, std::int32_t& error) {
    if (error) return nullptr;
    return static_cast<T*>(ft_mem_alloc_nid_postfix(memory, count * static_cast<std::int64_t>(sizeof(T)), &error));
}

template <typename T>
void Copy(T* target, const T* source, std::int64_t count) {
    if (count > 0) std::memcpy(target, source, static_cast<std::size_t>(count) * sizeof(T));
}

template <typename T>
void Replace(GuestMemoryRec* memory, T*& block, T* replacement) {
    ft_mem_free_nid_postfix(memory, block);
    block = replacement;
}

void AdjustPoints(GuestGlyphLoaderRec* loader) {
    GuestOutline& base = loader->base.outline;
    GuestOutline& current = loader->current.outline;
    current.points = base.points + Count(base.n_points);
    current.tags = base.tags + Count(base.n_points);
    current.contours = base.contours + Count(base.n_contours);
    if (loader->use_extra) {
        loader->current.extra_points = loader->base.extra_points + Count(base.n_points);
        loader->current.extra_points2 = loader->base.extra_points2 + Count(base.n_points);
    }
}

void AdjustSubglyphs(GuestGlyphLoaderRec* loader) {
    loader->current.subglyphs = static_cast<GuestSubGlyphRec*>(loader->base.subglyphs) + loader->base.num_subglyphs;
}

}

extern "C" {

std::int32_t APS5_VABI FT_GlyphLoader_New_nid_postfix(GuestMemoryRec* memory, GuestGlyphLoaderRec** aloader) {
    std::int32_t error = 0;
    auto* loader = static_cast<GuestGlyphLoaderRec*>(ft_mem_alloc_nid_postfix(memory, sizeof(GuestGlyphLoaderRec), &error));
    if (!error) {
        loader->memory = memory;
        *aloader = loader;
    }
    return error;
}

void APS5_VABI FT_GlyphLoader_Rewind_nid_postfix(GuestGlyphLoaderRec* loader) {
    Require(loader, __func__);
    loader->base.outline.n_points = 0;
    loader->base.outline.n_contours = 0;
    loader->base.num_subglyphs = 0;
    loader->current = loader->base;
}

void APS5_VABI FT_GlyphLoader_Reset_nid_postfix(GuestGlyphLoaderRec* loader) {
    Require(loader, __func__);
    GuestMemoryRec* memory = loader->memory;
    Free(memory, loader->base.outline.points);
    Free(memory, loader->base.outline.tags);
    Free(memory, loader->base.outline.contours);
    Free(memory, loader->base.extra_points);
    ft_mem_free_nid_postfix(memory, loader->base.subglyphs);
    loader->base.subglyphs = nullptr;
    loader->base.extra_points2 = nullptr;
    loader->max_points = 0;
    loader->max_contours = 0;
    loader->max_subglyphs = 0;
    FT_GlyphLoader_Rewind_nid_postfix(loader);
}

void APS5_VABI FT_GlyphLoader_Done_nid_postfix(GuestGlyphLoaderRec* loader) {
    if (!loader) return;
    GuestMemoryRec* memory = loader->memory;
    FT_GlyphLoader_Reset_nid_postfix(loader);
    ft_mem_free_nid_postfix(memory, loader);
}

std::int32_t APS5_VABI FT_GlyphLoader_CreateExtra_nid_postfix(GuestGlyphLoaderRec* loader) {
    Require(loader, __func__);
    std::int32_t error = 0;
    GuestVector* extra = Allocate<GuestVector>(loader->memory, 2 * static_cast<std::int64_t>(loader->max_points), error);
    if (!error) {
        Replace(loader->memory, loader->base.extra_points, extra);
        loader->use_extra = 1;
        loader->base.extra_points2 = loader->base.extra_points + loader->max_points;
        AdjustPoints(loader);
    }
    return error;
}

std::int32_t APS5_VABI FT_GlyphLoader_CheckPoints_nid_postfix(GuestGlyphLoaderRec* loader, std::uint32_t n_points, std::uint32_t n_contours) {
    Require(loader, __func__);
    GuestMemoryRec* memory = loader->memory;
    std::int32_t error = 0;
    GuestOutline& base = loader->base.outline;
    const GuestOutline& current = loader->current.outline;

    const std::int64_t points = Required(Count(base.n_points) + Count(current.n_points), n_points);
    const std::int64_t contours = Required(Count(base.n_contours) + Count(current.n_contours), n_contours);
    const std::int64_t pointsMax = points > loader->max_points ? PadCeil(points, 8) : 0;
    const std::int64_t contoursMax = contours > loader->max_contours ? PadCeil(contours, 4) : 0;
    if (pointsMax > OutlinePointsMax || contoursMax > OutlineContoursMax) return ArrayTooLarge;
    if (!pointsMax && !contoursMax) return 0;

    const std::int64_t oldPoints = loader->max_points;
    const bool extraTables = pointsMax && loader->use_extra;
    GuestVector* newPoints = pointsMax ? Allocate<GuestVector>(memory, pointsMax, error) : nullptr;
    unsigned char* newTags = pointsMax ? Allocate<unsigned char>(memory, pointsMax, error) : nullptr;
    GuestVector* newExtra = extraTables ? Allocate<GuestVector>(memory, 2 * pointsMax, error) : nullptr;
    std::uint16_t* newContours = contoursMax ? Allocate<std::uint16_t>(memory, contoursMax, error) : nullptr;
    if (error) {
        ft_mem_free_nid_postfix(memory, newPoints);
        ft_mem_free_nid_postfix(memory, newTags);
        ft_mem_free_nid_postfix(memory, newExtra);
        ft_mem_free_nid_postfix(memory, newContours);
        return error;
    }

    if (pointsMax) {
        Copy(newPoints, base.points, oldPoints);
        Copy(newTags, base.tags, oldPoints);
        Replace(memory, base.points, newPoints);
        Replace(memory, base.tags, newTags);
        if (extraTables) {
            Copy(newExtra, loader->base.extra_points, oldPoints);
            Copy(newExtra + pointsMax, loader->base.extra_points2, oldPoints);
            Replace(memory, loader->base.extra_points, newExtra);
            loader->base.extra_points2 = newExtra + pointsMax;
        }
        loader->max_points = static_cast<std::uint32_t>(pointsMax);
    }
    if (contoursMax) {
        Copy(newContours, base.contours, loader->max_contours);
        Replace(memory, base.contours, newContours);
        loader->max_contours = static_cast<std::uint32_t>(contoursMax);
    }
    AdjustPoints(loader);
    return 0;
}

std::int32_t APS5_VABI FT_GlyphLoader_CheckSubGlyphs_nid_postfix(GuestGlyphLoaderRec* loader, std::uint32_t n_subs) {
    Require(loader, __func__);
    std::int32_t error = 0;
    const std::int64_t subs = Required(static_cast<std::int64_t>(loader->base.num_subglyphs) + loader->current.num_subglyphs, n_subs);
    const std::uint32_t oldMax = loader->max_subglyphs;
    if (subs > oldMax) {
        const std::int64_t newMax = PadCeil(subs, 2);
        if (newMax > SubGlyphsMax) return ArrayTooLarge;
        loader->base.subglyphs = ft_mem_realloc_nid_postfix(loader->memory, sizeof(GuestSubGlyphRec), oldMax, newMax, loader->base.subglyphs, &error);
        if (error) return error;
        loader->max_subglyphs = static_cast<std::uint32_t>(newMax);
        AdjustSubglyphs(loader);
    }
    return error;
}

void APS5_VABI FT_GlyphLoader_Prepare_nid_postfix(GuestGlyphLoaderRec* loader) {
    Require(loader, __func__);
    loader->current.outline.n_points = 0;
    loader->current.outline.n_contours = 0;
    loader->current.num_subglyphs = 0;
    AdjustPoints(loader);
    AdjustSubglyphs(loader);
}

void APS5_VABI FT_GlyphLoader_Add_nid_postfix(GuestGlyphLoaderRec* loader) {
    if (!loader) return;
    GuestGlyphLoadRec& base = loader->base;
    GuestGlyphLoadRec& current = loader->current;
    const auto currentContours = static_cast<std::uint32_t>(Count(current.outline.n_contours));
    const auto basePoints = static_cast<std::uint32_t>(Count(base.outline.n_points));
    base.outline.n_points = static_cast<std::uint16_t>(base.outline.n_points + current.outline.n_points);
    base.outline.n_contours = static_cast<std::uint16_t>(base.outline.n_contours + current.outline.n_contours);
    base.num_subglyphs += current.num_subglyphs;
    for (std::uint32_t n = 0; n < currentContours; ++n) {
        current.outline.contours[n] = static_cast<std::uint16_t>(current.outline.contours[n] + basePoints);
    }
    FT_GlyphLoader_Prepare_nid_postfix(loader);
}

std::int32_t APS5_VABI FT_GlyphLoader_CopyPoints_nid_postfix(GuestGlyphLoaderRec* target, GuestGlyphLoaderRec* source) {
    Require(target, __func__);
    Require(source, __func__);
    const auto numPoints = static_cast<std::uint32_t>(Count(source->base.outline.n_points));
    const auto numContours = static_cast<std::uint32_t>(Count(source->base.outline.n_contours));
    const std::int32_t error = FT_GlyphLoader_CheckPoints_nid_postfix(target, numPoints, numContours);
    if (!error) {
        GuestOutline& out = target->base.outline;
        const GuestOutline& in = source->base.outline;
        std::memmove(out.points, in.points, numPoints * sizeof(GuestVector));
        std::memmove(out.tags, in.tags, numPoints);
        std::memmove(out.contours, in.contours, numContours * sizeof(std::uint16_t));
        if (target->use_extra && source->use_extra) {
            std::memmove(target->base.extra_points, source->base.extra_points, numPoints * sizeof(GuestVector));
            std::memmove(target->base.extra_points2, source->base.extra_points2, numPoints * sizeof(GuestVector));
        }
        out.n_points = static_cast<std::uint16_t>(numPoints);
        out.n_contours = static_cast<std::uint16_t>(numContours);
        AdjustPoints(target);
    }
    return error;
}

}
