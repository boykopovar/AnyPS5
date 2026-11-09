#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI sceHidControlGetControllerSerialNumberInformation(std::uint32_t, std::uint8_t*);
int APS5_VABI sceHidControlGetPswNearbyDeviceInfo(std::uint32_t, std::int64_t);
int APS5_VABI sceHidControlPswUpdateInit();
int APS5_VABI sceHidControlPswUpdateTerm();
int APS5_VABI sceHidControlSetPswControllerCommunicationMethod(std::uint32_t);
int APS5_VABI libSceHidControlUnknown_00qes5NswlQ_nid_no_patch();
int APS5_VABI libSceHidControlUnknown_8fIm7UR4VDc_nid_no_patch();
int APS5_VABI libSceHidControlUnknown_AjBZjowWaFI_nid_no_patch();
int APS5_VABI libSceHidControlUnknown_C05jP9w4PCc_nid_no_patch();
int APS5_VABI libSceHidControlUnknown_DUGI7AXFtV0_nid_no_patch();
int APS5_VABI libSceHidControlUnknown_HZJRMD3BeKk_nid_no_patch();
int APS5_VABI libSceHidControlUnknown_OFyJjudLKlA_nid_no_patch();
int APS5_VABI libSceHidControlUnknown_TKax3WNHtEg_nid_no_patch();
int APS5_VABI libSceHidControlUnknown_WikCB91EU0w_nid_no_patch();
int APS5_VABI libSceHidControlUnknown_i5ALHbvjRi8_nid_no_patch();
int APS5_VABI libSceHidControlUnknown_iVF5VrBHJzk_nid_no_patch();
int APS5_VABI libSceHidControlUnknown_tcDE01gUoN0_nid_no_patch();
int APS5_VABI libSceHidControlUnknown_w2MEBm2Tvm8_nid_no_patch();
int APS5_VABI libSceHidControlUnknown_yJJq5Arw8EI_nid_no_patch();

}
