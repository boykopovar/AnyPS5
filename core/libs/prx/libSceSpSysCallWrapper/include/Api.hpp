#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSpGetPid();
int APS5_VABI sceSpKernelGettimeofday();
int APS5_VABI sceSpKernelNanosleep();
int APS5_VABI sceSpNetConnect();
int APS5_VABI sceSpNetEpollAbort();
int APS5_VABI sceSpNetEpollControl();
int APS5_VABI sceSpNetEpollCreate();
int APS5_VABI sceSpNetEpollDestroy();
int APS5_VABI sceSpNetEpollWait();
int APS5_VABI sceSpNetErrnoLoc();
int APS5_VABI sceSpNetPoolCreate();
int APS5_VABI sceSpNetPoolDestroy();
int APS5_VABI sceSpNetRecv();
int APS5_VABI sceSpNetResolverAbort();
int APS5_VABI sceSpNetResolverCreate();
int APS5_VABI sceSpNetResolverDestroy();
int APS5_VABI sceSpNetResolverGetError();
int APS5_VABI sceSpNetResolverStartNtoa();
int APS5_VABI sceSpNetSend();
int APS5_VABI sceSpNetSetsockopt();
int APS5_VABI sceSpNetSocket();
int APS5_VABI sceSpNetSocketClose();
int APS5_VABI sceSpPthreadAttrDestroy();
int APS5_VABI sceSpPthreadAttrInit();
int APS5_VABI sceSpPthreadAttrSetinheritsched();
int APS5_VABI sceSpPthreadAttrSetschedparam();
int APS5_VABI sceSpPthreadAttrSetstacksize();
int APS5_VABI sceSpPthreadCondDestroy();
int APS5_VABI sceSpPthreadCondInit();
int APS5_VABI sceSpPthreadCondSignal();
int APS5_VABI sceSpPthreadCondWait();
int APS5_VABI sceSpPthreadCondattrDestroy();
int APS5_VABI sceSpPthreadCondattrInit();
int APS5_VABI sceSpPthreadCreate();
int APS5_VABI sceSpPthreadDetach();
int APS5_VABI sceSpPthreadExit();
int APS5_VABI sceSpPthreadJoin();
int APS5_VABI sceSpPthreadMutexDestroy();
int APS5_VABI sceSpPthreadMutexInit();
int APS5_VABI sceSpPthreadMutexLock();
int APS5_VABI sceSpPthreadMutexUnlock();
int APS5_VABI sceSpPthreadMutexattrDestroy();
int APS5_VABI sceSpPthreadMutexattrInit();
int APS5_VABI sceSpPthreadMutexattrSetprioceiling();
int APS5_VABI sceSpPthreadMutexattrSetprotocol();
int APS5_VABI sceSpPthreadMutexattrSettype();
int APS5_VABI sceSpPthreadSetprio();
int APS5_VABI sceSpSelect();

}
