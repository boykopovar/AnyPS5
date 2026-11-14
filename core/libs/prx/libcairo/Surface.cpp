#include "prx/libcairo/include/Surface.hpp"
#include "prx/libc/include/General.hpp"

#include <CairoBackend.h>

#include <memory>
#include <new>

namespace {

struct UserData {
    void* data;
    CairoDestroyFunc destroy;
};

cairo_surface_t* Backend(CairoSurface* surface) {
    return reinterpret_cast<cairo_surface_t*>(surface);
}

CairoSurface* Guest(cairo_surface_t* surface) {
    return reinterpret_cast<CairoSurface*>(surface);
}

void DestroyUserData(void* opaque) {
    const std::unique_ptr<UserData> userData(static_cast<UserData*>(opaque));
    userData->destroy(userData->data);
}

}

extern "C" {

int APS5_VABI cairo_format_stride_for_width(int format, int width) {
    return aps5_cairo_format_stride_for_width(static_cast<cairo_format_t>(format), width);
}

CairoSurface* APS5_VABI cairo_image_surface_create(int format, int width, int height) {
    return Guest(aps5_cairo_image_surface_create(static_cast<cairo_format_t>(format), width, height));
}

CairoSurface* APS5_VABI cairo_image_surface_create_for_data(unsigned char* data, int format,
                                                            int width, int height, int stride) {
    return Guest(aps5_cairo_image_surface_create_for_data(data, static_cast<cairo_format_t>(format),
                                                          width, height, stride));
}

unsigned char* APS5_VABI cairo_image_surface_get_data(CairoSurface* surface) {
    return aps5_cairo_image_surface_get_data(Backend(surface));
}

int APS5_VABI cairo_image_surface_get_format(CairoSurface* surface) {
    return aps5_cairo_image_surface_get_format(Backend(surface));
}

int APS5_VABI cairo_image_surface_get_width(CairoSurface* surface) {
    return aps5_cairo_image_surface_get_width(Backend(surface));
}

int APS5_VABI cairo_image_surface_get_height(CairoSurface* surface) {
    return aps5_cairo_image_surface_get_height(Backend(surface));
}

int APS5_VABI cairo_image_surface_get_stride(CairoSurface* surface) {
    return aps5_cairo_image_surface_get_stride(Backend(surface));
}

CairoSurface* APS5_VABI cairo_surface_create_similar(CairoSurface* other, int content,
                                                     int width, int height) {
    if (!other) NotImplemented_nid_no_patch(__func__);
    return Guest(aps5_cairo_surface_create_similar(Backend(other), static_cast<cairo_content_t>(content),
                                                   width, height));
}

CairoSurface* APS5_VABI cairo_surface_create_for_rectangle(CairoSurface* target,
                                                           double x, double y,
                                                           double width, double height) {
    if (!target) NotImplemented_nid_no_patch(__func__);
    return Guest(aps5_cairo_surface_create_for_rectangle(Backend(target), x, y, width, height));
}

CairoSurface* APS5_VABI cairo_surface_reference(CairoSurface* surface) {
    return Guest(aps5_cairo_surface_reference(Backend(surface)));
}

void APS5_VABI cairo_surface_destroy(CairoSurface* surface) {
    aps5_cairo_surface_destroy(Backend(surface));
}

void APS5_VABI cairo_surface_flush(CairoSurface* surface) {
    aps5_cairo_surface_flush(Backend(surface));
}

int APS5_VABI cairo_surface_get_content(CairoSurface* surface) {
    return aps5_cairo_surface_get_content(Backend(surface));
}

int APS5_VABI cairo_surface_get_type(CairoSurface* surface) {
    return aps5_cairo_surface_get_type(Backend(surface));
}

void APS5_VABI cairo_surface_get_device_scale(CairoSurface* surface, double* x_scale, double* y_scale) {
    aps5_cairo_surface_get_device_scale(Backend(surface), x_scale, y_scale);
}

void APS5_VABI cairo_surface_set_device_scale(CairoSurface* surface, double x_scale, double y_scale) {
    aps5_cairo_surface_set_device_scale(Backend(surface), x_scale, y_scale);
}

void APS5_VABI cairo_surface_mark_dirty_rectangle(CairoSurface* surface, int x, int y, int width, int height) {
    aps5_cairo_surface_mark_dirty_rectangle(Backend(surface), x, y, width, height);
}

int APS5_VABI cairo_surface_set_user_data(CairoSurface* surface, const CairoUserDataKey* key,
                                          void* data, CairoDestroyFunc destroy) {
    const auto* backendKey = reinterpret_cast<const cairo_user_data_key_t*>(key);
    if (!data || !destroy || aps5_cairo_surface_get_reference_count(Backend(surface)) == 0)
        return aps5_cairo_surface_set_user_data(Backend(surface), backendKey, data, nullptr);
    auto userData = std::unique_ptr<UserData>(new (std::nothrow) UserData{data, destroy});
    if (!userData) return CAIRO_STATUS_NO_MEMORY;
    const cairo_status_t status = aps5_cairo_surface_set_user_data(Backend(surface), backendKey,
                                                                   userData.get(), DestroyUserData);
    if (status == CAIRO_STATUS_SUCCESS) userData.release();
    return status;
}

int APS5_VABI cairo_surface_status(CairoSurface* surface) {
    if (!surface) return CAIRO_STATUS_NULL_POINTER;
    return aps5_cairo_surface_status(Backend(surface));
}

}
