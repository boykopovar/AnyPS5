#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceJemspaceCalloc();
int APS5_VABI sceJemspaceCreate();
int APS5_VABI sceJemspaceDestroy();
int APS5_VABI sceJemspaceFree();
int APS5_VABI sceJemspaceGetStats();
int APS5_VABI sceJemspaceMalloc();
int APS5_VABI sceJemspaceMallocUsableSize();
int APS5_VABI sceJemspaceMemalign();
int APS5_VABI sceJemspacePosixMemalign();
int APS5_VABI sceJemspacePrintStats();
int APS5_VABI sceJemspacePurge();
int APS5_VABI sceJemspaceRealloc();
int APS5_VABI sceJemspaceReallocalign();

}
