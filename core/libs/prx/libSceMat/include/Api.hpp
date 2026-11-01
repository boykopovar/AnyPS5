#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceMatMemoryPoolReserve(void);
int APS5_VABI sceMatUninitialize(void);
int APS5_VABI sceMatTagVirtualMemory(void);
int APS5_VABI sceMatMspaceCreate(void);
int APS5_VABI sceMatUnregisterResource(void);
int APS5_VABI sceMatMprotect(void);
int APS5_VABI sceMatMapDirectMemory(void);
int APS5_VABI sceMatInitialize(void);
int APS5_VABI sceMatNewFrame(void);
int APS5_VABI sceMatRegisterResourceType(void);
int APS5_VABI sceMatFree(void);
int APS5_VABI sceMatSetUserCallstackCapture(void);
int APS5_VABI sceMatMemoryPoolMove(void);
int APS5_VABI sceMatRegisterGroup(void);
int APS5_VABI sceMatMemoryPoolExpand(void);
int APS5_VABI sceMatMemoryPoolBatch(void);
int APS5_VABI sceMatAllocPoolMemory(void);
int APS5_VABI sceMatAgcUnregisterOwnerAndResources(void);
int APS5_VABI sceMatReleasePhysicalMemory(void);
int APS5_VABI sceMatMemoryPoolDecommit(void);
int APS5_VABI sceMatSetPrtAperture(void);
int APS5_VABI sceMatFreePoolMemory(void);
int APS5_VABI sceMatUnmapMemory(void);
int APS5_VABI sceMatMspaceDestroy(void);
int APS5_VABI sceMatAlloc(void);
int APS5_VABI sceMatReallocBegin(void);
int APS5_VABI sceMatReallocEnd(void);
int APS5_VABI sceMatAgcRegisterResource(void);
int APS5_VABI sceMatWriteBookmark(void);
int APS5_VABI sceMatBatchMap(void);
int APS5_VABI sceMatTagPool(void);
int APS5_VABI sceMatAgcRegisterOwner(void);
int APS5_VABI sceMatUnregisterAllResourcesForOwner(void);
int APS5_VABI sceMatMtypeprotect(void);
int APS5_VABI sceMatSetThreadInfo(void);
int APS5_VABI sceMatPushMarker(void);
int APS5_VABI sceMatUnregisterOwnerAndResources(void);
int APS5_VABI sceMatAgcUnregisterResource(void);
int APS5_VABI sceMatRegisterOwner(void);
int APS5_VABI sceMatAgcUnregisterAllResourcesForOwner(void);
int APS5_VABI sceMatPopMarker(void);
int APS5_VABI sceMatPushMarkerStatic(void);
int APS5_VABI sceMatUpdateModuleList(void);
int APS5_VABI sceMatResizeCommsBuffer(void);
int APS5_VABI sceMatReallocPoolMemory(void);
int APS5_VABI sceMatMemoryPoolCommit(void);
int APS5_VABI sceMatRegisterResource(void);
int APS5_VABI sceMatAllocPhysicalMemory(void);
int APS5_VABI sceMatReserveVirtualRange(void);
int APS5_VABI sceMatTagAllocation(void);
int APS5_VABI sceMatFreeRange(void);
int APS5_VABI sceMatCalloc(void);

}
