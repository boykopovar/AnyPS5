#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpAdvppBandwidthDownloadBegin();
int APS5_VABI sceNpAdvppBandwidthDownloadEnd();
int APS5_VABI sceNpBandwidthTestInitStartDownloadInternal();
int APS5_VABI sceNpBandwidthTestInitStartUploadInternal();

int APS5_VABI libSceNpUtilityUnknown_3BZG8C8D62Q_nid_no_patch();
int APS5_VABI libSceNpUtilityUnknown_VVM_plus_zY9FiEU_nid_no_patch();

}
