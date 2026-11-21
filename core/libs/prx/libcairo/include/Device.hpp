#pragma once

#include "SceTypes.hpp"

struct CairoDevice;

extern "C" void APS5_VABI cairo_device_destroy(CairoDevice* device);
