#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNgs2FftInit();
int APS5_VABI sceNgs2FftProcess();
int APS5_VABI sceNgs2FftQuerySize();
int APS5_VABI sceNgs2ParseWaveformUser(std::uintptr_t handler, std::uintptr_t userData, void* outInfo);
int APS5_VABI sceNgs2SystemEnumHandles(std::uintptr_t* outHandles, std::uint32_t maxHandles);
int APS5_VABI sceNgs2SystemEnumRackHandles(std::uintptr_t systemHandle, std::uintptr_t* outHandles, std::uint32_t maxHandles);

}
