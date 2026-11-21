#pragma once

#include <cstdint>
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceKernelCheckDipsw(std::uint16_t id);
int APS5_VABI sceKernelInitializeDipsw();
int APS5_VABI sceKernelIsAssistMode();
int APS5_VABI sceKernelIsDevelopmentMode();
int APS5_VABI sceKernelIsDisableRazor();
int APS5_VABI sceKernelGetDiableBinaryVersionCheckValue();
int APS5_VABI sceKernelReadDipswData();
int APS5_VABI sceKernelSetDipsw(std::uint16_t id);
int APS5_VABI sceKernelUnsetDipsw(std::uint16_t id);
int APS5_VABI sceKernelWriteDipswData();

}
