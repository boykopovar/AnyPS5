#pragma once

#include "SceTypes.hpp"

using NetCtlApCallback = void(APS5_VABI*)(int, void*);

extern "C" {

int APS5_VABI sceNetCtlApInit();
void APS5_VABI sceNetCtlApTerm();
int APS5_VABI sceNetCtlApCheckCallback();
int APS5_VABI sceNetCtlApClearEvent(int eventType);
int APS5_VABI sceNetCtlApRegisterCallback(NetCtlApCallback func, void* arg, int* cid);
int APS5_VABI sceNetCtlApUnregisterCallback(int cid);
int APS5_VABI sceNetCtlApGetResult(int eventType, int* errorCode);
int APS5_VABI sceNetCtlApGetInfo(void* info);
int APS5_VABI sceNetCtlApGetState(int* state);
int APS5_VABI sceNetCtlApStop();
int APS5_VABI sceNetCtlApGetConnectInfo(const char* optStr, void* connectInfo);

}
