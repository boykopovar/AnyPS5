#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVideoArbitrationAcceptEvent();
int APS5_VABI sceVideoArbitrationEnable();
int APS5_VABI sceVideoArbitrationInitialize();

}
