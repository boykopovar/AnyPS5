// Derived from FreeType src/base/ftobjs.c (FreeType 2.4.12, commit cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9).
// Copyright (C) 1996-2013 by David Turner, Robert Wilhelm, and Werner Lemberg.
// This file is part of the FreeType project, and may only be used, modified, and distributed
// under the terms of the FreeType project license, LICENSE.TXT. By continuing to use, modify,
// or distribute this file you indicate that you have read the license and understand and accept it fully.

#include "prx/libSceFreeTypeOt/include/SizeMetrics.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdint>
#include <cstring>
#include <limits>

using FreeType::GuestBitmapSize;
using FreeType::GuestFaceRec;
using FreeType::GuestSizeMetrics;
using FreeType::GuestSizeRequestRec;

namespace {

constexpr std::int32_t UnimplementedFeature = 0x07;
constexpr std::int32_t InvalidPixelSize = 0x17;
constexpr std::int32_t InvalidFaceHandle = 0x23;
constexpr std::int64_t FaceFlagScalable = 1;
constexpr std::int64_t FaceFlagFixedSizes = 2;
constexpr std::uint32_t RequestNominal = 0;
constexpr std::uint32_t RequestRealDim = 1;
constexpr std::uint32_t RequestBBox = 2;
constexpr std::uint32_t RequestCell = 3;
constexpr std::uint32_t RequestScales = 4;
constexpr std::uint64_t Int32Max = 0x7FFFFFFF;

std::uint64_t Magnitude(std::int64_t value) {
    return value < 0 ? 0 - static_cast<std::uint64_t>(value) : static_cast<std::uint64_t>(value);
}

std::int64_t MulFix(std::int64_t a, std::int64_t b, const char* function) {
    const std::uint64_t x = Magnitude(a);
    const std::uint64_t y = Magnitude(b);
    if (x > Int32Max || y > Int32Max) NotImplemented_nid_no_patch(function);
    const auto result = static_cast<std::int64_t>((x * y + 0x8000) >> 16);
    return (a < 0) != (b < 0) ? -result : result;
}

std::int64_t DivFix(std::int64_t a, std::int64_t b, const char* function) {
    const std::uint64_t x = Magnitude(a);
    const std::uint64_t y = Magnitude(b);
    if (x > Int32Max || y > Int32Max) NotImplemented_nid_no_patch(function);
    std::uint64_t quotient = Int32Max;
    if (y != 0) {
        const std::uint64_t numerator = (x << 16) + (y >> 1);
        if (x < 0x10000 && numerator > 0xFFFFFFFF) NotImplemented_nid_no_patch(function);
        quotient = numerator / y;
        if (quotient > Int32Max) NotImplemented_nid_no_patch(function);
    }
    const auto result = static_cast<std::int64_t>(quotient);
    return (a < 0) != (b < 0) ? -result : result;
}

std::int64_t MulDiv(std::int64_t a, std::int64_t b, std::int64_t c, const char* function) {
    const std::uint64_t x = Magnitude(a);
    const std::uint64_t y = Magnitude(b);
    const std::uint64_t z = Magnitude(c);
    if (x > Int32Max || y > Int32Max || z > Int32Max || (z == 0 && (x == 0 || y == 0))) {
        NotImplemented_nid_no_patch(function);
    }
    const std::uint64_t quotient = z == 0 ? Int32Max : (x * y + (z >> 1)) / z;
    if (quotient > 0xFFFFFFFF) NotImplemented_nid_no_patch(function);
    const auto result = static_cast<std::int64_t>(quotient);
    return ((a < 0) != (b < 0)) != (c < 0) ? -result : result;
}

std::int64_t Add(std::int64_t a, std::int64_t b, const char* function) {
    std::int64_t sum = 0;
    if (__builtin_add_overflow(a, b, &sum)) NotImplemented_nid_no_patch(function);
    return sum;
}

std::int64_t PixFloor(std::int64_t value) {
    return value & ~static_cast<std::int64_t>(63);
}

std::int64_t PixRound(std::int64_t value, const char* function) {
    return PixFloor(Add(value, 32, function));
}

std::int64_t PixCeil(std::int64_t value, const char* function) {
    return PixFloor(Add(value, 63, function));
}

std::uint16_t Ppem(std::int64_t value, const char* function) {
    return static_cast<std::uint16_t>(Add(value, 32, function) >> 6);
}

std::int64_t RequestDimension(std::int64_t size, std::uint32_t resolution, const char* function) {
    if (!resolution) return size;
    std::int64_t product = 0;
    if (__builtin_mul_overflow(size, static_cast<std::int64_t>(resolution), &product)) NotImplemented_nid_no_patch(function);
    return Add(product, 36, function) / 72;
}

void RecomputeScaled(const GuestFaceRec& face, GuestSizeMetrics& metrics, const char* function) {
    metrics.ascender = PixCeil(MulFix(face.ascender, metrics.y_scale, function), function);
    metrics.descender = PixFloor(MulFix(face.descender, metrics.y_scale, function));
    metrics.height = PixRound(MulFix(face.height, metrics.y_scale, function), function);
    metrics.max_advance = PixRound(MulFix(face.max_advance_width, metrics.x_scale, function), function);
}

GuestSizeMetrics& FaceMetrics(GuestFaceRec* face, const char* function) {
    if (!face || !face->size) NotImplemented_nid_no_patch(function);
    return face->size->metrics;
}

}

extern "C" {

void APS5_VABI FT_Select_Metrics_nid_postfix(GuestFaceRec* face, std::uint64_t strike_index) {
    GuestSizeMetrics& target = FaceMetrics(face, __func__);
    if (!face->available_sizes || face->num_fixed_sizes <= 0 || strike_index >= static_cast<std::uint64_t>(face->num_fixed_sizes)) {
        NotImplemented_nid_no_patch(__func__);
    }
    const GuestBitmapSize& bsize = face->available_sizes[strike_index];
    GuestSizeMetrics metrics = target;
    metrics.x_ppem = Ppem(bsize.x_ppem, __func__);
    metrics.y_ppem = Ppem(bsize.y_ppem, __func__);
    if (face->face_flags & FaceFlagScalable) {
        metrics.x_scale = DivFix(bsize.x_ppem, face->units_per_EM, __func__);
        metrics.y_scale = DivFix(bsize.y_ppem, face->units_per_EM, __func__);
        RecomputeScaled(*face, metrics, __func__);
    } else {
        metrics.x_scale = 1 << 16;
        metrics.y_scale = 1 << 16;
        metrics.ascender = bsize.y_ppem;
        metrics.descender = 0;
        metrics.height = static_cast<std::int64_t>(bsize.height) * 64;
        metrics.max_advance = bsize.x_ppem;
    }
    target = metrics;
}

void APS5_VABI FT_Request_Metrics_nid_postfix(GuestFaceRec* face, const GuestSizeRequestRec* req) {
    GuestSizeMetrics& target = FaceMetrics(face, __func__);
    if (!(face->face_flags & FaceFlagScalable)) {
        std::memset(&target, 0, sizeof(target));
        target.x_scale = 1 << 16;
        target.y_scale = 1 << 16;
        return;
    }
    if (!req) NotImplemented_nid_no_patch(__func__);
    GuestSizeMetrics metrics = target;
    std::int64_t scaledW = 0;
    std::int64_t scaledH = 0;
    if (req->type == RequestScales) {
        metrics.x_scale = req->width;
        metrics.y_scale = req->height;
        if (!metrics.x_scale) {
            metrics.x_scale = metrics.y_scale;
        } else if (!metrics.y_scale) {
            metrics.y_scale = metrics.x_scale;
        }
    } else {
        std::int64_t w = 0;
        std::int64_t h = 0;
        switch (req->type) {
        case RequestNominal:
            w = h = face->units_per_EM;
            break;
        case RequestRealDim:
            w = h = static_cast<std::int64_t>(face->ascender) - face->descender;
            break;
        case RequestBBox:
            if (__builtin_sub_overflow(face->bbox.xMax, face->bbox.xMin, &w) || __builtin_sub_overflow(face->bbox.yMax, face->bbox.yMin, &h)) {
                NotImplemented_nid_no_patch(__func__);
            }
            break;
        case RequestCell:
            w = face->max_advance_width;
            h = static_cast<std::int64_t>(face->ascender) - face->descender;
            break;
        default:
            break;
        }
        if (w == std::numeric_limits<std::int64_t>::min() || h == std::numeric_limits<std::int64_t>::min()) NotImplemented_nid_no_patch(__func__);
        if (w < 0) w = -w;
        if (h < 0) h = -h;
        scaledW = RequestDimension(req->width, req->horiResolution, __func__);
        scaledH = RequestDimension(req->height, req->vertResolution, __func__);
        if (req->width) {
            metrics.x_scale = DivFix(scaledW, w, __func__);
            if (req->height) {
                metrics.y_scale = DivFix(scaledH, h, __func__);
                if (req->type == RequestCell) {
                    if (metrics.y_scale > metrics.x_scale) {
                        metrics.y_scale = metrics.x_scale;
                    } else {
                        metrics.x_scale = metrics.y_scale;
                    }
                }
            } else {
                metrics.y_scale = metrics.x_scale;
                scaledH = MulDiv(scaledW, h, w, __func__);
            }
        } else {
            metrics.x_scale = metrics.y_scale = DivFix(scaledH, h, __func__);
            scaledW = MulDiv(scaledH, w, h, __func__);
        }
    }
    if (req->type != RequestNominal) {
        scaledW = MulFix(face->units_per_EM, metrics.x_scale, __func__);
        scaledH = MulFix(face->units_per_EM, metrics.y_scale, __func__);
    }
    metrics.x_ppem = Ppem(scaledW, __func__);
    metrics.y_ppem = Ppem(scaledH, __func__);
    RecomputeScaled(*face, metrics, __func__);
    target = metrics;
}

std::int32_t APS5_VABI FT_Match_Size_nid_postfix(GuestFaceRec* face, const GuestSizeRequestRec* req, unsigned char ignore_width, std::uint64_t* size_index) {
    if (!face) NotImplemented_nid_no_patch(__func__);
    if (!(face->face_flags & FaceFlagFixedSizes)) return InvalidFaceHandle;
    if (!req) NotImplemented_nid_no_patch(__func__);
    if (req->type != RequestNominal) return UnimplementedFeature;
    std::int64_t w = RequestDimension(req->width, req->horiResolution, __func__);
    std::int64_t h = RequestDimension(req->height, req->vertResolution, __func__);
    if (req->width && !req->height) {
        h = w;
    } else if (!req->width && req->height) {
        w = h;
    }
    w = PixRound(w, __func__);
    h = PixRound(h, __func__);
    if (face->num_fixed_sizes > 0 && !face->available_sizes) NotImplemented_nid_no_patch(__func__);
    for (std::int32_t i = 0; i < face->num_fixed_sizes; ++i) {
        const GuestBitmapSize& bsize = face->available_sizes[i];
        if (h != PixRound(bsize.y_ppem, __func__)) continue;
        if (ignore_width || w == PixRound(bsize.x_ppem, __func__)) {
            if (size_index) *size_index = static_cast<std::uint64_t>(i);
            return 0;
        }
    }
    return InvalidPixelSize;
}

}
