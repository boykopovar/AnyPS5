#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSystemLogger2DeliveryGetBatchResult();
int APS5_VABI sceSystemLogger2DeliveryGetResult();
int APS5_VABI sceSystemLogger2DeliverySetBatchDeliveryEvent();
int APS5_VABI sceSystemLogger2DeliveryTerminate();

}
