#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVideoCoreIfSetFunctionTableUnlimited(const void* table);
int APS5_VABI sceVideoCoreIfInitializeInterface(void* argument1, void* argument2);
int APS5_VABI sceVideoCoreIfMainLoop();
int APS5_VABI sceVideoCoreIfSetFunctionTable(const void* table);

}
