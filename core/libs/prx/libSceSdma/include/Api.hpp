#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSdmaBeginQueueMode(void);
int APS5_VABI sceSdmaCancelQueue(void);
int APS5_VABI sceSdmaClose(void);
int APS5_VABI sceSdmaConstantFill(void);
int APS5_VABI sceSdmaConstantFillNonBlocking(void);
int APS5_VABI sceSdmaCopyLinear(void);
int APS5_VABI sceSdmaCopyLinearNonBlocking(void);
int APS5_VABI sceSdmaCopyTiled(void);
int APS5_VABI sceSdmaCopyTiledNonBlocking(void);
int APS5_VABI sceSdmaCopyWindowL2L(void);
int APS5_VABI sceSdmaCopyWindowL2LNonBlocking(void);
int APS5_VABI sceSdmaCopyWindowT2T(void);
int APS5_VABI sceSdmaCopyWindowT2TNonBlocking(void);
int APS5_VABI sceSdmaCopyWindowTiled(void);
int APS5_VABI sceSdmaCopyWindowTiledNonBlocking(void);
int APS5_VABI sceSdmaEndQueueMode(void);
int APS5_VABI sceSdmaFenceNonBlocking(void);
int APS5_VABI sceSdmaFinalize(void);
int APS5_VABI sceSdmaFlushAll(void);
int APS5_VABI sceSdmaFlushAllWithEvent(void);
int APS5_VABI sceSdmaInitialize(void);
int APS5_VABI sceSdmaKickQueue(void);
int APS5_VABI sceSdmaOpen(void);
int APS5_VABI sceSdmaQueryWorkingBufferSize(void);
int APS5_VABI sceSdmaTryFlush(void);
int APS5_VABI sceSdmaWaitForEvent(void);

}
