#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_OUTLINE_H

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace FreeType;

namespace {

using GuestSpanFunction = void (APS5_VABI*)(int, int, const FT_Span*, void*);

struct SpanContext {
    GuestSpanFunction function;
    void* user;
};

void ForwardSpans(int y, int count, const FT_Span* spans, void* user) {
    const auto* context = static_cast<const SpanContext*>(user);
    context->function(y, count, spans, context->user);
}

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

bool CopyOut(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

bool Writable(void* destination, std::size_t bytes) {
    const int savedErrno = errno;
    const bool writable = GuestWritable_nid_no_patch(destination, bytes);
    errno = savedErrno;
    return writable;
}

FT_Pos HostCoordinate(std::int64_t value) {
    if (value < std::numeric_limits<FT_Pos>::min() || value > std::numeric_limits<FT_Pos>::max()) {
        throw std::runtime_error("FT_Outline_Render: a value outside the host FT_Pos range is not implemented");
    }
    return static_cast<FT_Pos>(value);
}

}

namespace FreeType {

RasterTargetSpan RasterTargetSpan_nid_no_patch(const GuestBitmap& target) {
    if (!target.buffer) return {};
    const std::int64_t pitch = target.pitch;
    const auto stride = static_cast<std::size_t>(pitch < 0 ? -pitch : pitch);
    return {static_cast<unsigned char*>(target.buffer), stride * target.rows};
}

}

extern "C" {

int APS5_VABI FT_Outline_Render_nid_postfix(GuestLibrary* library, GuestOutline* outline, GuestRasterParams* params) {
    FT_Library host = nullptr;
    if (!library || reinterpret_cast<std::uintptr_t>(library) % alignof(GuestLibrary) != 0 || !CopyIn(&host, &library->host, sizeof(host)) || !host) {
        return FT_Outline_Render(nullptr, nullptr, nullptr);
    }
    GuestOutline header{};
    if (!outline || !CopyIn(&header, outline, sizeof(header))) return FT_Outline_Render(host, nullptr, nullptr);
    std::vector<GuestVector> guestPoints(header.n_points);
    std::vector<unsigned char> tags(header.n_points);
    std::vector<std::uint16_t> contours(header.n_contours);
    if (!CopyIn(guestPoints.data(), header.points, guestPoints.size() * sizeof(GuestVector)) || !CopyIn(tags.data(), header.tags, tags.size()) ||
        !CopyIn(contours.data(), header.contours, contours.size() * sizeof(std::uint16_t))) {
        return FT_Outline_Render(host, nullptr, nullptr);
    }
    std::vector<FT_Vector> points;
    points.reserve(guestPoints.size());
    for (const GuestVector& point : guestPoints) points.push_back({HostCoordinate(point.x), HostCoordinate(point.y)});
    FT_Outline native{header.n_contours, header.n_points, points.data(), tags.data(), contours.data(), header.flags};
    GuestRasterParams raster{};
    if (!params || !CopyIn(&raster, params, sizeof(raster))) return FT_Outline_Render(host, &native, nullptr);
    if (raster.blackSpans || raster.bitTest || raster.bitSet) {
        throw std::runtime_error("FT_Outline_Render: black spans and bit callbacks are not implemented");
    }
    if (!Writable(params, sizeof(*params))) return FT_Err_Invalid_Argument;
    GuestBitmap guestTarget{};
    FT_Bitmap target{};
    FT_Raster_Params nativeParams{};
    nativeParams.flags = raster.flags;
    if (raster.target) {
        if (!CopyIn(&guestTarget, raster.target, sizeof(guestTarget))) return FT_Err_Invalid_Argument;
        target = {guestTarget.rows, guestTarget.width, guestTarget.pitch, guestTarget.buffer, guestTarget.num_grays,
                  guestTarget.pixel_mode, guestTarget.palette_mode, guestTarget.palette};
        nativeParams.target = &target;
    }
    SpanContext context{reinterpret_cast<GuestSpanFunction>(raster.graySpans), raster.user};
    if (raster.graySpans) {
        nativeParams.gray_spans = ForwardSpans;
        nativeParams.user = &context;
    } else {
        nativeParams.user = raster.user;
    }
    if (raster.flags & FT_RASTER_FLAG_CLIP) {
        nativeParams.clip_box = {HostCoordinate(raster.clipBox.xMin), HostCoordinate(raster.clipBox.yMin), HostCoordinate(raster.clipBox.xMax),
                                 HostCoordinate(raster.clipBox.yMax)};
    }
    const auto written = raster.target ? RasterTargetSpan_nid_no_patch(guestTarget) : RasterTargetSpan{};
    GuestArena::HostWrite write(written.begin, written.bytes);
    if (written.bytes != 0 && (!write.Open() || !Writable(written.begin, written.bytes))) return FT_Err_Invalid_Argument;
    const FT_Error error = FT_Outline_Render(host, &native, &nativeParams);
    if (nativeParams.source == &native) {
        const void* source = outline;
        if (!CopyOut(&params->source, &source, sizeof(source))) return FT_Err_Invalid_Argument;
        if (raster.flags & FT_RASTER_FLAG_DIRECT) {
            const GuestBBox clipBox{nativeParams.clip_box.xMin, nativeParams.clip_box.yMin, nativeParams.clip_box.xMax, nativeParams.clip_box.yMax};
            if (!CopyOut(&params->clipBox, &clipBox, sizeof(clipBox))) return FT_Err_Invalid_Argument;
        }
    }
    return error;
}

}
