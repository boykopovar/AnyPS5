#include "prx/libcairo/include/Surface.hpp"

#include <atomic>
#include <cstdint>
#include <limits>
#include <new>

namespace {

constexpr int kSuccess = 0;
constexpr int kNullPointer = 7;
constexpr int kInvalidContent = 15;
constexpr int kInvalidFormat = 16;
constexpr int kInvalidStride = 24;
constexpr int kInvalidSize = 32;
constexpr int kNoMemory = 1;
constexpr int kColor = 0x1000;
constexpr int kAlpha = 0x2000;
constexpr int kColorAlpha = 0x3000;
constexpr int kImageSurfaceType = 0;
constexpr int kInvalidFormatValue = -1;

int BitsPerPixel(int format) {
    switch (format) {
    case 0: case 1: case 5: return 32;
    case 2: return 8;
    case 3: return 1;
    case 4: return 16;
    case 6: return 96;
    case 7: return 128;
    default: return 0;
    }
}

int ContentForFormat(int format) {
    switch (format) {
    case 0: case 7: return kColorAlpha;
    case 2: case 3: return kAlpha;
    default: return kColor;
    }
}

int FormatForContent(int content) {
    switch (content) {
    case kColor: return 1;
    case kAlpha: return 2;
    case kColorAlpha: return 0;
    default: return -1;
    }
}

int Stride(int format, int width) {
    const int bits = BitsPerPixel(format);
    if (!bits || width < 0) return -1;

    const auto n = (static_cast<std::int64_t>(width) * bits + 31) / 32 * 4;
    return n > std::numeric_limits<int>::max() ? -1 : static_cast<int>(n);
}

}

struct CairoSurfaceState {
    std::atomic<unsigned> references{1};
    int status{kSuccess};
    int format{kInvalidFormatValue};
    int content{kColor};
    int width{};
    int height{};
    int stride{};
    unsigned char* data{};
    bool ownsData{};
    bool isImage{true};
    CairoSurface* parent{};
    double originX{};
    double originY{};
    double extentWidth{};
    double extentHeight{};
    double scaleX{1};
    double scaleY{1};

    ~CairoSurfaceState();
};

namespace {

CairoSurfaceState* State(CairoSurface* handle) {
    return reinterpret_cast<CairoSurfaceState*>(handle);
}
CairoSurface* Handle(CairoSurfaceState* state) {
    return reinterpret_cast<CairoSurface*>(state);
}
void ReleaseSurface(CairoSurface* surface);
}

CairoSurfaceState::~CairoSurfaceState() {
    if (ownsData) delete[] data;
    if (parent) ReleaseSurface(parent);
}

namespace {

void ReleaseSurface(CairoSurface* surface) {
    if (surface &&
        State(surface)->references.fetch_sub(1, std::memory_order_acq_rel) == 1)
        delete State(surface);
}

CairoSurfaceState* NewSurface(int status = kSuccess) {
    auto* surface = new (std::nothrow) CairoSurfaceState;
    if (surface) surface->status = status;
    return surface;
}

CairoSurface* ErrorSurface(int status) {
    return Handle(NewSurface(status));
}

CairoSurface* MakeImage(int format, int width, int height,
                        int suppliedStride, unsigned char* externalData, bool borrowed) {
    if (!BitsPerPixel(format)) return ErrorSurface(kInvalidFormat);
    const int minimumStride = Stride(format, width);

    if (minimumStride < 0 || width > 32767 || height < 0 || height > 32767)
        return ErrorSurface(kInvalidSize);
    if (borrowed && (suppliedStride < minimumStride || (suppliedStride & 3) != 0))
        return ErrorSurface(kInvalidStride);

    const int stride = borrowed ? suppliedStride : minimumStride;
    if (height && static_cast<std::uint64_t>(height) * stride >
                      std::numeric_limits<std::size_t>::max())
        return ErrorSurface(kInvalidSize);
    auto* result = NewSurface();
    if (!result) return nullptr;
    result->format = format;
    result->content = ContentForFormat(format);
    result->width = width;
    result->height = height;
    result->stride = stride;
    result->extentWidth = width;
    result->extentHeight = height;
    if (borrowed) {
        result->data = externalData;
    } else {
        const auto size = static_cast<std::size_t>(stride) * height;

        if (size) {
            result->data = new (std::nothrow) unsigned char[size]();
            if (!result->data) {
                delete result;
                return ErrorSurface(kNoMemory);
            }
            result->ownsData = true;
        }
    }
    return Handle(result);
}

}

extern "C" {

int APS5_VABI cairo_format_stride_for_width(int format, int width) {
    return Stride(format, width);
}

CairoSurface* APS5_VABI cairo_image_surface_create(int format, int width, int height) {
    return MakeImage(format, width, height, 0, nullptr, false);
}

CairoSurface* APS5_VABI cairo_image_surface_create_for_data(unsigned char* data, int format,
                                                            int width, int height, int stride) {
    return MakeImage(format, width, height, stride, data, true);
}

unsigned char* APS5_VABI cairo_image_surface_get_data(CairoSurface* surface) {
    return surface && State(surface)->status == kSuccess && State(surface)->isImage ? State(surface)->data : nullptr;
}

int APS5_VABI cairo_image_surface_get_format(CairoSurface* surface) {
    return surface && State(surface)->status == kSuccess && State(surface)->isImage
               ? State(surface)->format : kInvalidFormatValue;
}

int APS5_VABI cairo_image_surface_get_width(CairoSurface* surface) {
    return surface && State(surface)->status == kSuccess && State(surface)->isImage ? State(surface)->width : 0;
}

int APS5_VABI cairo_image_surface_get_height(CairoSurface* surface) {
    return surface && State(surface)->status == kSuccess && State(surface)->isImage ? State(surface)->height : 0;
}

int APS5_VABI cairo_image_surface_get_stride(CairoSurface* surface) {
    return surface && State(surface)->status == kSuccess && State(surface)->isImage ? State(surface)->stride : 0;
}

CairoSurface* APS5_VABI cairo_surface_reference(CairoSurface* surface) {
    if (surface)
        State(surface)->references.fetch_add(1, std::memory_order_relaxed);
    return surface;
}

void APS5_VABI cairo_surface_destroy(CairoSurface* surface) {

    ReleaseSurface(surface);
}

int APS5_VABI cairo_surface_status(CairoSurface* surface) {
    return surface ? State(surface)->status : kNullPointer;
}

int APS5_VABI cairo_surface_get_type(CairoSurface*) {
    return kImageSurfaceType;
}

int APS5_VABI cairo_surface_get_content(CairoSurface* surface) {
    return surface ? State(surface)->content : kColor;
}

void APS5_VABI cairo_surface_flush(CairoSurface*) {

}

void APS5_VABI cairo_surface_mark_dirty_rectangle(CairoSurface*, int, int, int, int) {

}

void APS5_VABI cairo_surface_set_device_scale(CairoSurface* surface, double x, double y) {
    if (!surface || State(surface)->status != kSuccess) return;
    State(surface)->scaleX = x;
    State(surface)->scaleY = y;
}

void APS5_VABI cairo_surface_get_device_scale(CairoSurface* surface, double* x, double* y) {
    if (x) *x = surface && State(surface)->status == kSuccess ? State(surface)->scaleX : 1;
    if (y) *y = surface && State(surface)->status == kSuccess ? State(surface)->scaleY : 1;
}

CairoSurface* APS5_VABI cairo_surface_create_similar(CairoSurface* other, int content,
                                                     int width, int height) {
    if (!other) return ErrorSurface(kNullPointer);
    if (State(other)->status != kSuccess) return ErrorSurface(State(other)->status);
    const int format = FormatForContent(content);
    if (format < 0) return ErrorSurface(kInvalidContent);
    auto* result = MakeImage(format, width, height, 0, nullptr, false);
    if (result && State(result)->status == kSuccess) {
        State(result)->scaleX = State(other)->scaleX;
        State(result)->scaleY = State(other)->scaleY;
    }
    return result;
}

CairoSurface* APS5_VABI cairo_surface_create_for_rectangle(CairoSurface* target,
                                                           double x, double y,
                                                           double width, double height) {
    if (!target) return ErrorSurface(kNullPointer);
    if (State(target)->status != kSuccess) return ErrorSurface(State(target)->status);
    if (!(width >= 0 && height >= 0)) return ErrorSurface(kInvalidSize);
    auto* result = NewSurface();
    if (!result) return nullptr;

    result->isImage = false;
    result->content = State(target)->content;
    result->parent = cairo_surface_reference(target);
    result->originX = x;
    result->originY = y;
    result->extentWidth = width;
    result->extentHeight = height;
    result->scaleX = State(target)->scaleX;
    result->scaleY = State(target)->scaleY;
    return Handle(result);
}

}
