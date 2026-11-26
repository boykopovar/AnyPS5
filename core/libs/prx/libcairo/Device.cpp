#include "prx/libcairo/include/Device.hpp"

#include <CairoBackend.h>

extern "C" void APS5_VABI cairo_device_destroy(CairoDevice* device) {
    aps5_cairo_device_destroy(reinterpret_cast<cairo_device_t*>(device));
}
