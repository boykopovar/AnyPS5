#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSystemGestureSetTouchAreaInformation(int32_t gestureHandle, const void* information);

}
