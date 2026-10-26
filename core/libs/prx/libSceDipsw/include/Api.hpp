#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceKernelCheckDipsw();
int APS5_VABI sceKernelInitializeDipsw();
int APS5_VABI sceKernelIsAssistMode();
int APS5_VABI sceKernelIsDevelopmentMode();
int APS5_VABI sceKernelIsDisableRazor();
int APS5_VABI sceKernelReadDipswData();
int APS5_VABI sceKernelSetDipsw();
int APS5_VABI sceKernelUnsetDipsw();
int APS5_VABI sceKernelWriteDipswData();

}
