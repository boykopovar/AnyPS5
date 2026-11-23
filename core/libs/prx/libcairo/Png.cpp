#include "prx/libcairo/include/Surface.hpp"

#include <CairoBackend.h>

namespace {

struct WriteStream {
    CairoWriteFunc write;
    void* closure;
};

struct ReadStream {
    CairoReadFunc read;
    void* closure;
};

cairo_status_t Write(void* opaque, const unsigned char* data, unsigned int length) {
    const auto& stream = *static_cast<WriteStream*>(opaque);
    return static_cast<cairo_status_t>(stream.write(stream.closure, data, length));
}

cairo_status_t Read(void* opaque, unsigned char* data, unsigned int length) {
    const auto& stream = *static_cast<ReadStream*>(opaque);
    return static_cast<cairo_status_t>(stream.read(stream.closure, data, length));
}

}

extern "C" {

int APS5_VABI cairo_surface_write_to_png_stream(CairoSurface* surface, CairoWriteFunc write, void* closure) {
    WriteStream stream{write, closure};
    return aps5_cairo_surface_write_to_png_stream(reinterpret_cast<cairo_surface_t*>(surface), Write, &stream);
}

CairoSurface* APS5_VABI cairo_image_surface_create_from_png_stream(CairoReadFunc read, void* closure) {
    ReadStream stream{read, closure};
    return reinterpret_cast<CairoSurface*>(aps5_cairo_image_surface_create_from_png_stream(Read, &stream));
}

}
