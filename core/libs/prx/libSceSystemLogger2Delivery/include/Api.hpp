#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSystemLogger2DeliveryGetBatchResult();
int APS5_VABI sceSystemLogger2DeliveryGetResult();
int APS5_VABI sceSystemLogger2DeliverySetBatchDeliveryEvent();
int APS5_VABI sceSystemLogger2DeliveryTerminate();

int APS5_VABI sceSystemLogger2DeliveryResume();
int APS5_VABI sceSystemLogger2DeliverySuspend();
int APS5_VABI sceSystemLogger2DeliveryInitialize();

}
