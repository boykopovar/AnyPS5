#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceUpsrvDisableAutoInstall();
int APS5_VABI sceUpsrvEndUpdateSession();
int APS5_VABI sceUpsrvNotifyUpdatedEvent();
int APS5_VABI sceUpsrvUpdateCheckDoCheckSystemWithDownloadTypeForSettings();
int APS5_VABI sceUpsrvUpdateCheckDoCheckSystemWithReleaseType();
int APS5_VABI sceUpsrvUpdateCheckIsTestPeriod();
int APS5_VABI sceUpsrvUpdateConfirmUpdate();
int APS5_VABI sceUpsrvUpdateStartUpdateModeWithParam();
int APS5_VABI sceUpsrvUpdateStartUpdateMode();
int APS5_VABI libSceUpdateServiceUnknown_33Z2z4kXKq0_nid_no_patch();
int APS5_VABI libSceUpdateServiceUnknown_ItlDmxWppec_nid_no_patch();
int APS5_VABI libSceUpdateServiceUnknown_SpnzKCdKA_plus_4_nid_no_patch();
int APS5_VABI libSceUpdateServiceUnknown_sEcvo34_plus_ihg_nid_no_patch();
int APS5_VABI libSceUpdateServiceUnknown_vrw9HD_minus_1Gs0_nid_no_patch();
int APS5_VABI libSceUpdateServiceUnknown_yyy3cEojLOw_nid_no_patch();

}
