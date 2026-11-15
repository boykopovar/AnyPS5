#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceCLCClearFileList();
int APS5_VABI sceCLCClearTrackList();
int APS5_VABI sceCLCGetDeviceInfo();
int APS5_VABI sceCLCGetDeviceList();
int APS5_VABI sceCLCGetFileInfo();
int APS5_VABI sceCLCGetFileList();
int APS5_VABI sceCLCGetFileListInfo();
int APS5_VABI sceCLCGetFolderInfo();
int APS5_VABI sceCLCGetShuffle();
int APS5_VABI sceCLCGetTrackInfo();
int APS5_VABI sceCLCGetTrackList();
int APS5_VABI sceCLCGetTrackListInfo();
int APS5_VABI sceCLCGetUsbStorageCallback();
int APS5_VABI sceCLCRegisterEventCallback();
int APS5_VABI sceCLCSetPlaybackResult();
int APS5_VABI sceCLCSetShuffle();
int APS5_VABI sceCLCSetTrackList();
int APS5_VABI sceCLCSetUsbStorageCallback();
int APS5_VABI libSceContentListControllerUnknown_SoBk_minus_kuNLsE_nid_no_patch();

}
