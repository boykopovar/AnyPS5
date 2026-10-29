#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSystemLogger2NativeQueueClientCheckAndMaskData();
int APS5_VABI sceSystemLogger2NativeQueueClientGetEvent();
int APS5_VABI sceSystemLogger2NativeQueueClientSetDeliveryEnable();
int APS5_VABI sceSystemLogger2NativeQueueClientSetDeliveryResult();
int APS5_VABI sceSystemLogger2NativeQueueClientSetNativeQueueConfig();

}
