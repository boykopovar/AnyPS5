#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpAdvppBandwidthDownloadBegin();
int APS5_VABI sceNpAdvppBandwidthDownloadEnd();
int APS5_VABI sceNpBandwidthTestInitStartDownloadInternal();
int APS5_VABI sceNpBandwidthTestInitStartUploadInternal();

}
