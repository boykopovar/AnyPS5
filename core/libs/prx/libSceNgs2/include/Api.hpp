#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNgs2.native/include/Ngs2Types.hpp"

extern "C" {

int APS5_VABI sceNgs2FftInit(void* work, std::size_t size, int length, std::uint32_t flags, void** handle);
int APS5_VABI sceNgs2FftProcess(void* handle, const float* in, float* out, float* work, std::uintptr_t unused, std::uint32_t mode);
int APS5_VABI sceNgs2FftQuerySize(std::uint32_t length, std::uint32_t flags, std::uint64_t* size);
int APS5_VABI sceNgs2ParseWaveformData(const void* data, std::size_t dataSize, Ngs2WaveformInfo* outInfo);
int APS5_VABI sceNgs2ParseWaveformUser(std::uintptr_t handler, std::uintptr_t userData, Ngs2WaveformInfo* outInfo);
int APS5_VABI sceNgs2SystemEnumHandles(std::uintptr_t* outHandles, std::uint32_t maxHandles);
int APS5_VABI sceNgs2SystemEnumRackHandles(std::uintptr_t systemHandle, std::uintptr_t* outHandles, std::uint32_t maxHandles);

}
