#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpTcsIntDownloadData();
int APS5_VABI sceNpTcsIntUploadDataToNewSlotAbort();
int APS5_VABI sceNpTcsIntUploadDataToNewSlotBegin();

}
