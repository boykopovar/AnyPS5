#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI GetContentFileStatusLib();
int APS5_VABI GetPhotoInfoLib();
int APS5_VABI GetUsbThumbnailEntryListLib();
int APS5_VABI InitializeLib();
int APS5_VABI TerminateLib();

}
