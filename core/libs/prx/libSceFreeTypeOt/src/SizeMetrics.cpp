// Adapted from https://github.com/freetype/freetype/blob/37515cf56ba3349656049edbed89ffc6f6466f2d/src/base/ftobjs.c
/****************************************************************************
 *
 * ftobjs.c
 *
 *   The FreeType private base classes (body).
 *
 * Copyright (C) 1996-2026 by
 * David Turner, Robert Wilhelm, and Werner Lemberg.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.  By continuing to use, modify, or distribute
 * this file you indicate that you have read the license and
 * understand and accept it fully.
 *
 */

#include "prx/libSceFreeTypeOt/include/SizeMetrics.hpp"
#include "prx/libc/include/General.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

using FreeType::GuestBitmapSize;
using FreeType::GuestFaceRec;
using FreeType::GuestSizeMetrics;
using FreeType::GuestSizeRequestRec;
using FreeType::GuestSizeRec;

namespace {

FT_Long MetricLong(std::int64_t value, const char* function) {
    if (value < std::numeric_limits<FT_Long>::min() || value > std::numeric_limits<FT_Long>::max()) {
        throw std::invalid_argument(std::string(function) + ": metric outside host FT_Long");
    }
    return static_cast<FT_Long>(value);
}

FT_Long PixelFloor(FT_Long value) {
    return value & ~FT_Long{63};
}

FT_Long PixelRound(FT_Long value) {
    if (value > std::numeric_limits<FT_Long>::max() - 32) throw std::invalid_argument("FT_Select_Metrics: rounded metric overflow");
    return PixelFloor(value + 32);
}

FT_Long PixelCeil(FT_Long value) {
    if (value > std::numeric_limits<FT_Long>::max() - 63) throw std::invalid_argument("FT_Select_Metrics: rounded metric overflow");
    return PixelFloor(value + 63);
}

std::int64_t MatchAdd(std::int64_t left, std::int64_t right, const char* function) {
    std::int64_t result = 0;
    if (__builtin_add_overflow(left, right, &result)) NotImplemented_nid_no_patch(function);
    return result;
}

std::int64_t MatchRequestDimension(std::int64_t value, std::uint32_t resolution, const char* function) {
    if (!resolution) return value;
    std::int64_t product = 0;
    if (__builtin_mul_overflow(value, static_cast<std::int64_t>(resolution), &product)) NotImplemented_nid_no_patch(function);
    return MatchAdd(product, 36, function) / 72;
}

std::int64_t MatchRound(std::int64_t value, const char* function) {
    return MatchAdd(value, 32, function) & ~std::int64_t{63};
}

bool MatchReadStrike(const GuestFaceRec& face, std::int32_t index, GuestBitmapSize& strike, const char* function) {
    std::uintptr_t offset = 0;
    std::uintptr_t address = 0;
    if (__builtin_mul_overflow(static_cast<std::uintptr_t>(index), sizeof(strike), &offset) ||
        __builtin_add_overflow(reinterpret_cast<std::uintptr_t>(face.available_sizes), offset, &address)) {
        NotImplemented_nid_no_patch(function);
    }
    return GuestCopyIn_nid_no_patch(&strike, reinterpret_cast<const void*>(address), sizeof(strike));
}

FT_Long RequestLong(std::int64_t value, const char* function) {
    if (value < std::numeric_limits<FT_Long>::min() || value > std::numeric_limits<FT_Long>::max()) {
        throw std::invalid_argument(std::string(function) + ": metric outside host FT_Long");
    }
    return static_cast<FT_Long>(value);
}

FT_Long RequestDifference(FT_Long left, FT_Long right, const char* function) {
    if ((right > 0 && left < std::numeric_limits<FT_Long>::min() + right) ||
        (right < 0 && left > std::numeric_limits<FT_Long>::max() + right)) {
        throw std::invalid_argument(std::string(function) + ": metric difference overflow");
    }
    return left - right;
}

FT_Long RequestMagnitude(FT_Long value, const char* function) {
    if (value == std::numeric_limits<FT_Long>::min()) {
        throw std::invalid_argument(std::string(function) + ": metric magnitude overflow");
    }
    return value < 0 ? -value : value;
}

bool RequestProductOverflows(std::int64_t value, std::uint32_t factor) {
    if (!factor || !value) return false;
    const auto signedFactor = static_cast<std::int64_t>(factor);
    if (value > 0) return value > std::numeric_limits<std::int64_t>::max() / signedFactor;
    return value < std::numeric_limits<std::int64_t>::min() / signedFactor;
}

void ValidateRequest(const GuestSizeRequestRec& request, const char* function) {
    if (RequestProductOverflows(request.width, request.horiResolution) || RequestProductOverflows(request.height, request.vertResolution) ||
        request.width > std::numeric_limits<std::int64_t>::max() - 64 || request.height > std::numeric_limits<std::int64_t>::max() - 64) {
        throw std::invalid_argument(std::string(function) + ": request arithmetic overflow");
    }
}

FT_Long ScaledRequest(FT_Long value, std::uint32_t resolution, const char* function) {
    if (!resolution) return value;
    std::int64_t product = 0;
    if (__builtin_mul_overflow(static_cast<std::int64_t>(value), static_cast<std::int64_t>(resolution), &product) ||
        product > std::numeric_limits<std::int64_t>::max() - 36) {
        throw std::invalid_argument(std::string(function) + ": scaled request overflow");
    }
    return RequestLong((product + 36) / 72, function);
}

FT_Long RequestPixelFloor(FT_Long value) {
    return value & ~FT_Long{63};
}

FT_Long RequestPixelRound(FT_Long value, const char* function) {
    if (value > std::numeric_limits<FT_Long>::max() - 32) {
        throw std::invalid_argument(std::string(function) + ": rounded metric overflow");
    }
    return RequestPixelFloor(value + 32);
}

FT_Long RequestPixelCeil(FT_Long value, const char* function) {
    if (value > std::numeric_limits<FT_Long>::max() - 63) {
        throw std::invalid_argument(std::string(function) + ": rounded metric overflow");
    }
    return RequestPixelFloor(value + 63);
}

void RecomputeRequestMetrics(const GuestFaceRec& face, GuestSizeMetrics& metrics, FT_Long xScale, FT_Long yScale, const char* function) {
    metrics.x_scale = xScale;
    metrics.y_scale = yScale;
    metrics.ascender = RequestPixelCeil(FT_MulFix(face.ascender, yScale), function);
    metrics.descender = RequestPixelFloor(FT_MulFix(face.descender, yScale));
    metrics.height = RequestPixelRound(FT_MulFix(face.height, yScale), function);
    metrics.max_advance = RequestPixelRound(FT_MulFix(face.max_advance_width, xScale), function);
}

std::int32_t RequestMetrics(GuestFaceRec* face, const GuestSizeRequestRec* requestAddress, const char* function) {
    if (!face) throw std::invalid_argument(std::string(function) + ": null face");
    GuestFaceRec header{};
    if (!GuestCopyIn_nid_no_patch(&header, face, sizeof(header))) return FT_Err_Invalid_Argument;
    if (!header.size) throw std::invalid_argument(std::string(function) + ": null size");
    GuestSizeMetrics metrics{};
    if (!GuestCopyIn_nid_no_patch(&metrics, &header.size->metrics, sizeof(metrics))) return FT_Err_Invalid_Argument;
    FT_Error error = FT_Err_Ok;
    if (header.face_flags & FT_FACE_FLAG_SCALABLE) {
        if (!requestAddress) throw std::invalid_argument(std::string(function) + ": null request");
        GuestSizeRequestRec request{};
        if (!GuestCopyIn_nid_no_patch(&request, requestAddress, sizeof(request))) return FT_Err_Invalid_Argument;
        ValidateRequest(request, function);
        const FT_Long requestWidth = RequestLong(request.width, function);
        const FT_Long requestHeight = RequestLong(request.height, function);
        FT_Long width = 0;
        FT_Long height = 0;
        FT_Long scaledWidth = 0;
        FT_Long scaledHeight = 0;
        FT_Long xScale = RequestLong(metrics.x_scale, function);
        FT_Long yScale = RequestLong(metrics.y_scale, function);
        switch (request.type) {
        case FT_SIZE_REQUEST_TYPE_NOMINAL:
            width = height = header.units_per_EM;
            break;
        case FT_SIZE_REQUEST_TYPE_REAL_DIM:
            width = height = RequestDifference(header.ascender, header.descender, function);
            break;
        case FT_SIZE_REQUEST_TYPE_BBOX:
            width = RequestDifference(RequestLong(header.bbox.xMax, function), RequestLong(header.bbox.xMin, function), function);
            height = RequestDifference(RequestLong(header.bbox.yMax, function), RequestLong(header.bbox.yMin, function), function);
            break;
        case FT_SIZE_REQUEST_TYPE_CELL:
            width = header.max_advance_width;
            height = RequestDifference(header.ascender, header.descender, function);
            break;
        case FT_SIZE_REQUEST_TYPE_SCALES:
            xScale = requestWidth;
            yScale = requestHeight;
            if (!xScale) xScale = yScale;
            else if (!yScale) yScale = xScale;
            metrics.x_scale = xScale;
            metrics.y_scale = yScale;
            goto CalculatePpem;
        case FT_SIZE_REQUEST_TYPE_MAX:
        default:
            break;
        }
        width = RequestMagnitude(width, function);
        height = RequestMagnitude(height, function);
        scaledWidth = ScaledRequest(requestWidth, request.horiResolution, function);
        scaledHeight = ScaledRequest(requestHeight, request.vertResolution, function);
        if (request.height || !request.width) {
            if (!height) {
                error = FT_Err_Divide_By_Zero;
                goto Store;
            }
            yScale = FT_DivFix(scaledHeight, height);
            metrics.y_scale = yScale;
        }
        if (request.width) {
            if (!width) {
                error = FT_Err_Divide_By_Zero;
                goto Store;
            }
            xScale = FT_DivFix(scaledWidth, width);
            metrics.x_scale = xScale;
        } else {
            xScale = yScale;
            metrics.x_scale = xScale;
            scaledWidth = FT_MulDiv(scaledHeight, width, height);
        }
        if (!request.height) {
            yScale = xScale;
            metrics.y_scale = yScale;
            scaledHeight = FT_MulDiv(scaledWidth, height, width);
        }
        if (request.type == FT_SIZE_REQUEST_TYPE_CELL) {
            if (yScale > xScale) yScale = xScale;
            else xScale = yScale;
            metrics.x_scale = xScale;
            metrics.y_scale = yScale;
        }
    CalculatePpem:
        if (request.type != FT_SIZE_REQUEST_TYPE_NOMINAL) {
            scaledWidth = FT_MulFix(header.units_per_EM, xScale);
            scaledHeight = FT_MulFix(header.units_per_EM, yScale);
        }
        if (scaledWidth > std::numeric_limits<FT_Long>::max() - 32 || scaledHeight > std::numeric_limits<FT_Long>::max() - 32) {
            throw std::invalid_argument(std::string(function) + ": ppem rounding overflow");
        }
        scaledWidth = (scaledWidth + 32) / 64;
        scaledHeight = (scaledHeight + 32) / 64;
        if (scaledWidth > std::numeric_limits<FT_UShort>::max() || scaledHeight > std::numeric_limits<FT_UShort>::max()) {
            error = FT_Err_Invalid_Pixel_Size;
            goto Store;
        }
        metrics.x_ppem = static_cast<std::uint16_t>(scaledWidth);
        metrics.y_ppem = static_cast<std::uint16_t>(scaledHeight);
        RecomputeRequestMetrics(header, metrics, xScale, yScale, function);
    } else {
        metrics = {};
        metrics.x_scale = 1L << 16;
        metrics.y_scale = 1L << 16;
    }
Store:
    if (!GuestCopyOut_nid_no_patch(&header.size->metrics, &metrics, sizeof(metrics))) return FT_Err_Invalid_Argument;
    return error;
}

}

extern "C" void APS5_VABI FT_Select_Metrics_nid_postfix(GuestFaceRec* face, std::uint64_t strikeIndex) {
    if (!face) throw std::invalid_argument(std::string(__func__) + ": null face");
    GuestFaceRec header{};
    if (!GuestCopyIn_nid_no_patch(&header, face, sizeof(header))) throw std::invalid_argument(std::string(__func__) + ": unreadable face");
    if (!header.size) throw std::invalid_argument(std::string(__func__) + ": null size");
    if (header.num_fixed_sizes <= 0 || strikeIndex >= static_cast<std::uint64_t>(header.num_fixed_sizes) || !header.available_sizes) {
        throw std::invalid_argument(std::string(__func__) + ": invalid strike index or table");
    }
    const auto base = reinterpret_cast<std::uintptr_t>(header.available_sizes);
    constexpr std::uintptr_t stride = sizeof(GuestBitmapSize);
    if (strikeIndex > (std::numeric_limits<std::uintptr_t>::max() - base) / stride) {
        throw std::invalid_argument(std::string(__func__) + ": strike address overflow");
    }
    GuestBitmapSize strike{};
    const auto* strikeAddress = reinterpret_cast<const GuestBitmapSize*>(base + static_cast<std::uintptr_t>(strikeIndex) * stride);
    if (!GuestCopyIn_nid_no_patch(&strike, strikeAddress, sizeof(strike))) throw std::invalid_argument(std::string(__func__) + ": unreadable strike");
    GuestSizeMetrics metrics{};
    const FT_Long xPpem = MetricLong(strike.x_ppem, __func__);
    const FT_Long yPpem = MetricLong(strike.y_ppem, __func__);
    if (xPpem > std::numeric_limits<FT_Long>::max() - 32 || yPpem > std::numeric_limits<FT_Long>::max() - 32) {
        throw std::invalid_argument(std::string(__func__) + ": ppem rounding overflow");
    }
    metrics.x_ppem = static_cast<std::uint16_t>((xPpem + 32) >> 6);
    metrics.y_ppem = static_cast<std::uint16_t>((yPpem + 32) >> 6);
    if (header.face_flags & FT_FACE_FLAG_SCALABLE) {
        const FT_Long xScale = FT_DivFix(xPpem, header.units_per_EM);
        const FT_Long yScale = FT_DivFix(yPpem, header.units_per_EM);
        metrics.x_scale = xScale;
        metrics.y_scale = yScale;
        metrics.ascender = PixelCeil(FT_MulFix(header.ascender, yScale));
        metrics.descender = PixelFloor(FT_MulFix(header.descender, yScale));
        metrics.height = PixelRound(FT_MulFix(header.height, yScale));
        metrics.max_advance = PixelRound(FT_MulFix(header.max_advance_width, xScale));
    } else {
        metrics.x_scale = 1L << 16;
        metrics.y_scale = 1L << 16;
        metrics.ascender = yPpem;
        metrics.descender = 0;
        metrics.height = static_cast<std::int64_t>(strike.height) * 64;
        metrics.max_advance = xPpem;
    }
    const auto sizeAddress = reinterpret_cast<std::uintptr_t>(header.size);
    constexpr std::uintptr_t metricsOffset = offsetof(FreeType::GuestSizeRec, metrics);
    if (sizeAddress > std::numeric_limits<std::uintptr_t>::max() - metricsOffset) {
        throw std::invalid_argument(std::string(__func__) + ": size metrics address overflow");
    }
    auto* destination = reinterpret_cast<GuestSizeMetrics*>(sizeAddress + metricsOffset);
    if (!GuestCopyOut_nid_no_patch(destination, &metrics, sizeof(metrics))) {
        throw std::invalid_argument(std::string(__func__) + ": unwritable size metrics");
    }
}

extern "C" std::int32_t APS5_VABI FT_Match_Size_nid_postfix(GuestFaceRec* face, const GuestSizeRequestRec* request, unsigned char ignoreWidth,
    std::uint64_t* sizeIndex) {
    if (!face) NotImplemented_nid_no_patch(__func__);
    GuestFaceRec header{};
    if (!GuestCopyIn_nid_no_patch(&header, face, sizeof(header))) return FT_Err_Invalid_Argument;
    if (!(header.face_flags & FT_FACE_FLAG_FIXED_SIZES)) return FT_Err_Invalid_Face_Handle;
    if (!request) NotImplemented_nid_no_patch(__func__);
    GuestSizeRequestRec guestRequest{};
    if (!GuestCopyIn_nid_no_patch(&guestRequest, request, sizeof(guestRequest))) return FT_Err_Invalid_Argument;
    if (guestRequest.type != FT_SIZE_REQUEST_TYPE_NOMINAL) return FT_Err_Unimplemented_Feature;
    std::int64_t width = MatchRequestDimension(guestRequest.width, guestRequest.horiResolution, __func__);
    std::int64_t height = MatchRequestDimension(guestRequest.height, guestRequest.vertResolution, __func__);
    if (guestRequest.width && !guestRequest.height) {
        height = width;
    } else if (!guestRequest.width && guestRequest.height) {
        width = height;
    }
    width = MatchRound(width, __func__);
    height = MatchRound(height, __func__);
    if (!width || !height) return FT_Err_Invalid_Pixel_Size;
    if (header.num_fixed_sizes > 0 && !header.available_sizes) NotImplemented_nid_no_patch(__func__);
    for (std::int32_t index = 0; index < header.num_fixed_sizes; ++index) {
        GuestBitmapSize strike{};
        if (!MatchReadStrike(header, index, strike, __func__)) return FT_Err_Invalid_Argument;
        if (height != MatchRound(strike.y_ppem, __func__)) continue;
        if (width == MatchRound(strike.x_ppem, __func__) || ignoreWidth) {
            const std::uint64_t result = static_cast<std::uint64_t>(index);
            if (sizeIndex && !GuestCopyOut_nid_no_patch(sizeIndex, &result, sizeof(result))) return FT_Err_Invalid_Argument;
            return FT_Err_Ok;
        }
    }
    return FT_Err_Invalid_Pixel_Size;
}

extern "C" std::int32_t APS5_VABI FT_Request_Metrics_nid_postfix(GuestFaceRec* face, const GuestSizeRequestRec* request) {
    return RequestMetrics(face, request, __func__);
}
