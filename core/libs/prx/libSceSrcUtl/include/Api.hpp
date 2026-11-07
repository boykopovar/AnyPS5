#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSrcUtilityCreateEx();
int APS5_VABI sceSrcUtilityDeleteEx();
int APS5_VABI sceSrcUtilityFinalize();
int APS5_VABI sceSrcUtilityFlush();
int APS5_VABI sceSrcUtilityGetAjmContext();
int APS5_VABI sceSrcUtilityGetDelaySamples();
int APS5_VABI sceSrcUtilityGetHandleSize(std::uint32_t channels, std::uint32_t mode, std::uint32_t input_rate, std::uint32_t output_rate);
int APS5_VABI sceSrcUtilityGetPriority(const void* handle, std::uint32_t* priority);
int APS5_VABI sceSrcUtilityInitialize();
int APS5_VABI sceSrcUtilityInitializeEx();
int APS5_VABI sceSrcUtilityResample();
int APS5_VABI sceSrcUtilitySetPriority(void* handle, std::uint32_t priority);
int APS5_VABI sceSrcUtilityTerminateEx();

}
