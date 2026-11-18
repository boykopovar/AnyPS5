#include "prx/libSceFreeTypeOt/include/SizeMetrics.hpp"
#include "prx/libc/include/General.hpp"

#if defined(ANYPS5_FREETYPEOT_SYSTEM_DEPS)
extern "C" {

void APS5_VABI FT_Select_Metrics_nid_postfix(FreeType::GuestFaceRec*, std::uint64_t) {
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI FT_Request_Metrics_nid_postfix(FreeType::GuestFaceRec*, const FreeType::GuestSizeRequestRec*) {
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI FT_Match_Size_nid_postfix(FreeType::GuestFaceRec*, const FreeType::GuestSizeRequestRec*, unsigned char, std::uint64_t*) {
    NotImplemented_nid_no_patch(__func__);
}

}
#else
#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype/internal/ftobjs.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using FreeType::GuestBitmapSize;
using FreeType::GuestFaceRec;
using FreeType::GuestSizeMetrics;
using FreeType::GuestSizeRec;
using FreeType::GuestSizeRequestRec;

namespace {

constexpr std::size_t Page = 0x1000;

bool CopyStrikes(std::vector<GuestBitmapSize>& strikes, const GuestBitmapSize* source, std::int32_t count) {
    const std::size_t total = static_cast<std::size_t>(count) * sizeof(GuestBitmapSize);
    const auto* from = reinterpret_cast<const unsigned char*>(source);
    std::vector<unsigned char> bytes;
    while (bytes.size() < total) {
        const std::size_t offset = bytes.size();
        const auto address = reinterpret_cast<std::uintptr_t>(from) + offset;
        const std::size_t chunk = (std::min)(total - offset, Page - address % Page);
        bytes.resize(offset + chunk);
        if (!GuestCopyIn_nid_no_patch(bytes.data() + offset, from + offset, chunk)) return false;
    }
    strikes.resize(static_cast<std::size_t>(count));
    std::memcpy(strikes.data(), bytes.data(), total);
    return true;
}

FT_Long Narrow(std::int64_t value, const char* function) {
    if (value < std::numeric_limits<FT_Long>::min() || value > std::numeric_limits<FT_Long>::max()) NotImplemented_nid_no_patch(function);
    return static_cast<FT_Long>(value);
}

void RequireNoOverflow(const GuestSizeRequestRec& req, const char* function) {
    std::int64_t product = 0;
    if (__builtin_mul_overflow(req.width, static_cast<std::int64_t>(req.horiResolution), &product) ||
        __builtin_mul_overflow(req.height, static_cast<std::int64_t>(req.vertResolution), &product) ||
        req.width > std::numeric_limits<std::int64_t>::max() - 64 || req.height > std::numeric_limits<std::int64_t>::max() - 64) {
        NotImplemented_nid_no_patch(function);
    }
}

FT_Size_RequestRec HostRequest(const GuestSizeRequestRec& req, const char* function) {
    RequireNoOverflow(req, function);
    return {static_cast<FT_Size_Request_Type>(req.type), Narrow(req.width, function), Narrow(req.height, function), req.horiResolution, req.vertResolution};
}

struct HostFace {
    FT_FaceRec face{};
    FT_SizeRec size{};
    std::vector<FT_Bitmap_Size> strikes;

    HostFace(const GuestFaceRec& guest, const char* function) {
        face.face_flags = Narrow(guest.face_flags, function);
        face.units_per_EM = guest.units_per_EM;
        face.ascender = guest.ascender;
        face.descender = guest.descender;
        face.height = guest.height;
        face.max_advance_width = guest.max_advance_width;
        std::int64_t width = 0;
        std::int64_t height = 0;
        if (__builtin_sub_overflow(guest.bbox.xMax, guest.bbox.xMin, &width) || __builtin_sub_overflow(guest.bbox.yMax, guest.bbox.yMin, &height) ||
            width == std::numeric_limits<std::int64_t>::min() || height == std::numeric_limits<std::int64_t>::min()) {
            NotImplemented_nid_no_patch(function);
        }
        face.bbox = {Narrow(guest.bbox.xMin, function), Narrow(guest.bbox.yMin, function), Narrow(guest.bbox.xMax, function), Narrow(guest.bbox.yMax, function)};
        if (guest.num_fixed_sizes > 0) {
            if (!guest.available_sizes) NotImplemented_nid_no_patch(function);
            std::vector<GuestBitmapSize> guestStrikes;
            if (!CopyStrikes(guestStrikes, guest.available_sizes, guest.num_fixed_sizes)) {
                readable = false;
                return;
            }
            for (const GuestBitmapSize& strike : guestStrikes) {
                if (strike.x_ppem > std::numeric_limits<std::int64_t>::max() - 64 || strike.y_ppem > std::numeric_limits<std::int64_t>::max() - 64) {
                    NotImplemented_nid_no_patch(function);
                }
                strikes.push_back({strike.height, strike.width, Narrow(strike.size, function), Narrow(strike.x_ppem, function), Narrow(strike.y_ppem, function)});
            }
        }
        face.num_fixed_sizes = guest.num_fixed_sizes;
        face.available_sizes = strikes.data();
        face.size = &size;
        size.face = &face;
        if (!guest.size) return;
        if (!GuestCopyIn_nid_no_patch(&guestSize, guest.size, sizeof(guestSize))) {
            readable = false;
            return;
        }
        const GuestSizeMetrics& metrics = guestSize.metrics;
        size.metrics = {metrics.x_ppem, metrics.y_ppem, Narrow(metrics.x_scale, function), Narrow(metrics.y_scale, function),
            Narrow(metrics.ascender, function), Narrow(metrics.descender, function), Narrow(metrics.height, function), Narrow(metrics.max_advance, function)};
    }

    bool Readable() const {
        return readable;
    }

    bool Store(GuestSizeRec* target) const {
        GuestSizeMetrics metrics = guestSize.metrics;
        metrics.x_ppem = size.metrics.x_ppem;
        metrics.y_ppem = size.metrics.y_ppem;
        metrics.x_scale = size.metrics.x_scale;
        metrics.y_scale = size.metrics.y_scale;
        metrics.ascender = size.metrics.ascender;
        metrics.descender = size.metrics.descender;
        metrics.height = size.metrics.height;
        metrics.max_advance = size.metrics.max_advance;
        return GuestCopyOut_nid_no_patch(&target->metrics, &metrics, sizeof(metrics));
    }

private:
    GuestSizeRec guestSize{};
    bool readable = true;
};

}

extern "C" {

void APS5_VABI FT_Select_Metrics_nid_postfix(GuestFaceRec* face, std::uint64_t strike_index) {
    if (!face) NotImplemented_nid_no_patch(__func__);
    GuestFaceRec header{};
    if (!GuestCopyIn_nid_no_patch(&header, face, sizeof(header))) throw std::invalid_argument(std::string(__func__) + ": unreadable face");
    if (!header.size || header.num_fixed_sizes <= 0 || strike_index >= static_cast<std::uint64_t>(header.num_fixed_sizes)) {
        NotImplemented_nid_no_patch(__func__);
    }
    HostFace host(header, __func__);
    if (!host.Readable()) throw std::invalid_argument(std::string(__func__) + ": unreadable size or strikes");
    FT_Select_Metrics(&host.face, static_cast<FT_ULong>(strike_index));
    if (!host.Store(header.size)) throw std::invalid_argument(std::string(__func__) + ": unwritable size metrics");
}

std::int32_t APS5_VABI FT_Request_Metrics_nid_postfix(GuestFaceRec* face, const GuestSizeRequestRec* req) {
    if (!face) NotImplemented_nid_no_patch(__func__);
    GuestFaceRec header{};
    if (!GuestCopyIn_nid_no_patch(&header, face, sizeof(header))) return FT_Err_Invalid_Argument;
    if (!header.size) NotImplemented_nid_no_patch(__func__);
    HostFace host(header, __func__);
    if (!host.Readable()) return FT_Err_Invalid_Argument;
    FT_Size_RequestRec request{};
    if (FT_IS_SCALABLE(&host.face)) {
        if (!req) NotImplemented_nid_no_patch(__func__);
        GuestSizeRequestRec guestRequest{};
        if (!GuestCopyIn_nid_no_patch(&guestRequest, req, sizeof(guestRequest))) return FT_Err_Invalid_Argument;
        request = HostRequest(guestRequest, __func__);
    }
    const FT_Error error = FT_Request_Metrics(&host.face, &request);
    if (!host.Store(header.size)) return FT_Err_Invalid_Argument;
    return error;
}

std::int32_t APS5_VABI FT_Match_Size_nid_postfix(GuestFaceRec* face, const GuestSizeRequestRec* req, unsigned char ignore_width, std::uint64_t* size_index) {
    if (!face) NotImplemented_nid_no_patch(__func__);
    GuestFaceRec header{};
    if (!GuestCopyIn_nid_no_patch(&header, face, sizeof(header))) return FT_Err_Invalid_Argument;
    if (!(header.face_flags & FT_FACE_FLAG_FIXED_SIZES)) return FT_Err_Invalid_Face_Handle;
    if (!req) NotImplemented_nid_no_patch(__func__);
    GuestSizeRequestRec guestRequest{};
    if (!GuestCopyIn_nid_no_patch(&guestRequest, req, sizeof(guestRequest))) return FT_Err_Invalid_Argument;
    if (guestRequest.type != FT_SIZE_REQUEST_TYPE_NOMINAL) return FT_Err_Unimplemented_Feature;
    if (size_index && !GuestWritable_nid_no_patch(size_index, sizeof(*size_index))) return FT_Err_Invalid_Argument;
    HostFace host(header, __func__);
    if (!host.Readable()) return FT_Err_Invalid_Argument;
    FT_Size_RequestRec request = HostRequest(guestRequest, __func__);
    FT_ULong index = 0;
    const FT_Error error = FT_Match_Size(&host.face, &request, ignore_width, &index);
    if (!error && size_index) {
        const std::uint64_t outIndex = index;
        if (!GuestCopyOut_nid_no_patch(size_index, &outIndex, sizeof(outIndex))) return FT_Err_Invalid_Argument;
    }
    return error;
}

}
#endif
