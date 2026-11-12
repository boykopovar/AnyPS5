#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSystemLogger2NativeQueueClientCheckAndMaskData();
int APS5_VABI sceSystemLogger2NativeQueueClientGetEvent(void* event);
int APS5_VABI sceSystemLogger2NativeQueueClientSetDeliveryEnable();
int APS5_VABI sceSystemLogger2NativeQueueClientSetDeliveryResult();
int APS5_VABI sceSystemLogger2NativeQueueClientSetNativeQueueConfig();

int APS5_VABI sceSystemLogger2NativeQueueClientSetBatchDeliveryResult();
int APS5_VABI sceSystemLogger2NativeQueueClientSetEventFilter();
int APS5_VABI sceSystemLogger2NativeQueueClientCheckData(const void* data, void* result);
int APS5_VABI sceSystemLogger2NativeQueueClientGetEvent2();
int APS5_VABI sceSystemLogger2NativeQueueClientInitialize();
int APS5_VABI sceSystemLogger2NativeQueueClientTerminate();

int APS5_VABI libSceSystemLogger2NativeQueueClientUnknown_AuwwRl6zAcU_nid_no_patch();

}
