#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_OUTLINE_H

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
    FT_Library host = library ? library->host : nullptr;
    if (!library || !outline) return FT_Outline_Render(host, nullptr, nullptr);
    std::vector<FT_Vector> points;
    points.reserve(outline->n_points);
    for (std::size_t index = 0; index < outline->n_points; ++index) {
        points.push_back({HostCoordinate(outline->points[index].x), HostCoordinate(outline->points[index].y)});
    }
    FT_Outline native{outline->n_contours, outline->n_points, points.data(), outline->tags, outline->contours, outline->flags};
    if (!params) return FT_Outline_Render(host, &native, nullptr);
    if (params->blackSpans || params->bitTest || params->bitSet) {
        throw std::runtime_error("FT_Outline_Render: black spans and bit callbacks are not implemented");
    }
    FT_Bitmap target{};
    FT_Raster_Params nativeParams{};
    nativeParams.flags = params->flags;
    if (params->target) {
        target = {params->target->rows, params->target->width, params->target->pitch, params->target->buffer, params->target->num_grays,
                  params->target->pixel_mode, params->target->palette_mode, params->target->palette};
        nativeParams.target = &target;
    }
    SpanContext context{reinterpret_cast<GuestSpanFunction>(params->graySpans), params->user};
    if (params->graySpans) {
        nativeParams.gray_spans = ForwardSpans;
        nativeParams.user = &context;
    } else {
        nativeParams.user = params->user;
    }
    if (params->flags & FT_RASTER_FLAG_CLIP) {
        nativeParams.clip_box = {HostCoordinate(params->clipBox.xMin), HostCoordinate(params->clipBox.yMin), HostCoordinate(params->clipBox.xMax),
                                 HostCoordinate(params->clipBox.yMax)};
    }
    const auto written = params->target ? RasterTargetSpan_nid_no_patch(*params->target) : RasterTargetSpan{};
    GuestArena::HostWrite write(written.begin, written.bytes);
    const FT_Error error = FT_Outline_Render(host, &native, &nativeParams);
    if (nativeParams.source == &native) {
        params->source = outline;
        if (params->flags & FT_RASTER_FLAG_DIRECT) {
            params->clipBox = {nativeParams.clip_box.xMin, nativeParams.clip_box.yMin, nativeParams.clip_box.xMax, nativeParams.clip_box.yMax};
        }
    }
    return error;
}

}
