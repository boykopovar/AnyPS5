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

std::int32_t Count(std::uint16_t value) {
    return static_cast<std::int16_t>(value);
}

std::uint32_t PadCeil(std::uint32_t value, std::uint32_t step) {
    return (value + step - 1) & ~(step - 1);
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
bool Renew(GuestMemoryRec* memory, T*& block, std::int64_t current, std::int64_t count, std::int32_t& error) {
    block = static_cast<T*>(ft_mem_realloc_nid_postfix(memory, sizeof(T), current, count, block, &error));
    return error != 0;
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
    loader->current.subglyphs = loader->base.subglyphs + loader->base.num_subglyphs;
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
    Free(memory, loader->base.subglyphs);
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
    if (!Renew(loader->memory, loader->base.extra_points, 0, 2 * static_cast<std::int64_t>(loader->max_points), error)) {
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
    bool adjust = false;

    std::uint32_t newMax = static_cast<std::uint32_t>(Count(base.n_points) + Count(current.n_points)) + n_points;
    std::uint32_t oldMax = loader->max_points;
    if (newMax > oldMax) {
        newMax = PadCeil(newMax, 8);
        if (newMax > OutlinePointsMax) return ArrayTooLarge;
        if (Renew(memory, base.points, oldMax, newMax, error) || Renew(memory, base.tags, oldMax, newMax, error)) return error;
        if (loader->use_extra) {
            if (Renew(memory, loader->base.extra_points, 2 * static_cast<std::int64_t>(oldMax), 2 * static_cast<std::int64_t>(newMax), error)) {
                return error;
            }
            std::memmove(loader->base.extra_points + newMax, loader->base.extra_points + oldMax, oldMax * sizeof(GuestVector));
            loader->base.extra_points2 = loader->base.extra_points + newMax;
        }
        adjust = true;
        loader->max_points = newMax;
    }

    oldMax = loader->max_contours;
    newMax = static_cast<std::uint32_t>(Count(base.n_contours) + Count(current.n_contours)) + n_contours;
    if (newMax > oldMax) {
        newMax = PadCeil(newMax, 4);
        if (newMax > OutlineContoursMax) return ArrayTooLarge;
        if (Renew(memory, base.contours, oldMax, newMax, error)) return error;
        adjust = true;
        loader->max_contours = newMax;
    }

    if (adjust) AdjustPoints(loader);
    return error;
}

std::int32_t APS5_VABI FT_GlyphLoader_CheckSubGlyphs_nid_postfix(GuestGlyphLoaderRec* loader, std::uint32_t n_subs) {
    Require(loader, __func__);
    std::int32_t error = 0;
    std::uint32_t newMax = loader->base.num_subglyphs + loader->current.num_subglyphs + n_subs;
    const std::uint32_t oldMax = loader->max_subglyphs;
    if (newMax > oldMax) {
        newMax = PadCeil(newMax, 2);
        if (Renew(loader->memory, loader->base.subglyphs, oldMax, newMax, error)) return error;
        loader->max_subglyphs = newMax;
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
        std::memcpy(out.points, in.points, numPoints * sizeof(GuestVector));
        std::memcpy(out.tags, in.tags, numPoints);
        std::memcpy(out.contours, in.contours, numContours * sizeof(std::uint16_t));
        if (target->use_extra && source->use_extra) {
            std::memcpy(target->base.extra_points, source->base.extra_points, numPoints * sizeof(GuestVector));
            std::memcpy(target->base.extra_points2, source->base.extra_points2, numPoints * sizeof(GuestVector));
        }
        out.n_points = static_cast<std::uint16_t>(numPoints);
        out.n_contours = static_cast<std::uint16_t>(numContours);
        AdjustPoints(target);
    }
    return error;
}

}
