#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceSrcUtilityCreateEx(const std::uint32_t* context, std::uint32_t channels, std::uint32_t mode, std::uint32_t inputRate, std::uint32_t outputRate, void* handle);
int APS5_VABI sceSrcUtilityDeleteEx(void* handle);
int APS5_VABI sceSrcUtilityFinalize(void* handle);
int APS5_VABI sceSrcUtilityFlush(void* handle, std::int32_t argument1, void* argument2, void* argument3);
int APS5_VABI sceSrcUtilityGetAjmContext(const void* handle, std::uint32_t* context);
int APS5_VABI sceSrcUtilityGetDelaySamples();
int APS5_VABI sceSrcUtilityGetHandleSize(std::uint32_t channels, std::uint32_t mode, std::uint32_t input_rate, std::uint32_t output_rate);
int APS5_VABI sceSrcUtilityGetPriority(const void* handle, std::uint32_t* priority);
int APS5_VABI sceSrcUtilityInitialize(std::uint32_t channels, std::uint32_t mode, std::uint32_t inputRate, std::uint32_t outputRate, void* handle);
int APS5_VABI sceSrcUtilityInitializeEx(std::uint32_t* context);
int APS5_VABI sceSrcUtilityResample(void* handle, std::int32_t argument1);
int APS5_VABI sceSrcUtilitySetPriority(void* handle, std::uint32_t priority);
int APS5_VABI sceSrcUtilityTerminateEx();

}
