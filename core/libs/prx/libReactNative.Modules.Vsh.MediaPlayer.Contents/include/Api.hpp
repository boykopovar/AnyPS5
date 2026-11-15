#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI InitializeLib_nid_postfix(void);
int APS5_VABI TerminateLib_nid_postfix(void);
int APS5_VABI GetContentFileStatusLib_nid_postfix(void);
int APS5_VABI GetUsbEntryListLib_nid_postfix(void);
int APS5_VABI GetUsbThumbnailEntryListLib_nid_postfix(void);
int APS5_VABI reactNative_Modules_Vsh_MediaPlayer_ContentsUnknown00(void);
int APS5_VABI GetVideoInfoLib_nid_postfix(void);
int APS5_VABI GetPhotoInfoLib_nid_postfix(void);
int APS5_VABI reactNative_Modules_Vsh_MediaPlayer_ContentsUnknown01(void);

}
