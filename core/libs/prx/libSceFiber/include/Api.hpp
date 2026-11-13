#pragma once

#include "SceTypes.hpp"

using GuestFiberEntry = FiberEntry;

extern "C" {

int32_t APS5_VABI _sceFiberInitializeImpl_nid_postfix(FiberObject* object, const char* name, GuestFiberEntry entry, uint64_t argOnInitialize, void* addrContext, uint64_t sizeContext, const void* optParam, uint32_t buildVersion);
int32_t APS5_VABI sceFiberFinalize(FiberObject* object);
int32_t APS5_VABI sceFiberRun_nid_postfix(FiberObject* object, uint64_t argOnRun, uint64_t* argOnReturn);
int32_t APS5_VABI sceFiberSwitch(FiberObject* object, uint64_t argOnRun, uint64_t* argOnRunOut);
int32_t APS5_VABI sceFiberReturnToThread(uint64_t argOnReturn, uint64_t* argOnRun);
int32_t APS5_VABI sceFiberGetSelf(FiberObject** fiber);
int32_t APS5_VABI sceFiberGetInfo(FiberObject* object, FiberInfo* fiberInfo);
int32_t APS5_VABI sceFiberRename(FiberObject* object, const char* name);
int32_t APS5_VABI sceFiberOptParamInitialize(FiberOptParam* optParam);
int32_t APS5_VABI sceFiberGetThreadFramePointerAddress(uint64_t* addrFramePointer);
int32_t APS5_VABI _sceFiberGetThreadFramePointerAddress(uint64_t* addrFramePointer);
int32_t APS5_VABI sceFiberStartContextSizeCheck(uint32_t flags);
int32_t APS5_VABI sceFiberStopContextSizeCheck(void);
int32_t APS5_VABI _sceFiberAttachContextAndRun(FiberObject* object, void* addrContext, uint64_t sizeContext, uint64_t argOnRun, uint64_t* argOnReturn);
int32_t APS5_VABI _sceFiberAttachContextAndSwitch(FiberObject* object, void* addrContext, uint64_t sizeContext, uint64_t argOnRun, uint64_t* argOnRunOut);
int32_t APS5_VABI _sceFiberInitializeWithInternalOptionImpl(FiberObject* object, const char* name, GuestFiberEntry entry, uint64_t argOnInitialize, void* addrContext, uint64_t sizeContext, const FiberOptParam* optParam, uint32_t flags, uint32_t buildVersion);

}
