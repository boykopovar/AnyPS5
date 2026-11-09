#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVideoCoreIfSetFunctionTableUnlimited();
int APS5_VABI sceVideoCoreIfInitializeInterface();
int APS5_VABI sceVideoCoreIfMainLoop();
int APS5_VABI sceVideoCoreIfSetFunctionTable();

}
