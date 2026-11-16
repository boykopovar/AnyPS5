#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceMusicCoreIfInitializeInterface(const void* argument1, const void* param);
int APS5_VABI sceMusicCoreIfSetFunctionTable(const void* table);
int APS5_VABI sceMusicCoreIfMainLoop();
int APS5_VABI sceMusicCoreIfSetInternalFunctionTable(const void* table);
int APS5_VABI sceMusicCoreIfSetFunctionTableExt2(const void* table);
int APS5_VABI sceMusicCoreIfSetFunctionTableUnlimited(const void* table);
int APS5_VABI sceMusicCoreIfInternalInitializeInterface(const char* name, const void* argument2);

}
