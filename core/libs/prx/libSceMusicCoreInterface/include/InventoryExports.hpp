#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceMusicCoreIfInitializeInterface();
int APS5_VABI sceMusicCoreIfSetFunctionTable();
int APS5_VABI sceMusicCoreIfMainLoop();
int APS5_VABI sceMusicCoreIfSetInternalFunctionTable();
int APS5_VABI sceMusicCoreIfSetFunctionTableExt2();
int APS5_VABI sceMusicCoreIfSetFunctionTableUnlimited();
int APS5_VABI sceMusicCoreIfInternalInitializeInterface();

}
