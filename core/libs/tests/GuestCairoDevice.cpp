#include "prx/libcairo/include/Device.hpp"

#include <CairoBackend.h>

#include <cstdio>
#include <cstdlib>

namespace {

void Check(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "cairo device lifetime mismatch\n");
        std::abort();
    }
}

void Destroy(void* value) {
    ++*static_cast<int*>(value);
}

}

int main() {
    cairo_device_destroy(nullptr);
    cairo_surface_t* image = aps5_cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 8, 8);
    cairo_surface_t* observer = aps5_cairo_surface_create_observer(image, CAIRO_SURFACE_OBSERVER_RECORD_OPERATIONS);
    Check(aps5_cairo_surface_status(observer) == CAIRO_STATUS_SUCCESS);
    cairo_device_t* device = aps5_cairo_surface_get_device(observer);
    Check(device != nullptr);
    cairo_user_data_key_t key{};
    int destroyed = 0;
    Check(aps5_cairo_device_set_user_data(device, &key, &destroyed, Destroy) == CAIRO_STATUS_SUCCESS);
    aps5_cairo_device_reference(device);
    cairo_device_destroy(reinterpret_cast<CairoDevice*>(device));
    Check(destroyed == 0 && aps5_cairo_device_get_reference_count(device) == 1);
    aps5_cairo_device_reference(device);
    aps5_cairo_surface_destroy(observer);
    Check(destroyed == 0);
    cairo_device_destroy(reinterpret_cast<CairoDevice*>(device));
    Check(destroyed == 1);
    aps5_cairo_surface_destroy(image);
}
