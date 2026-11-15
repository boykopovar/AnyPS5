#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpTcsIntUploadDataToNewSlot();
int APS5_VABI sceNpTcsIntCreateContext();
int APS5_VABI sceNpTcsIntUploadDataToNewSlotEnd();
int APS5_VABI sceNpTcsIntDestroyContext();
int APS5_VABI sceNpTcsIntAbortRequest();
int APS5_VABI sceNpTcsIntDestroyRequest();
int APS5_VABI sceNpTcsIntUploadDataToNewSlotWrite();
int APS5_VABI sceNpTcsIntCreateRequest();

}
