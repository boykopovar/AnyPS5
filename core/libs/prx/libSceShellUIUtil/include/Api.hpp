#pragma once

#include "SceTypes.hpp"

struct ShellUIUtilLaunchParameter;

extern "C" {

int APS5_VABI sceShellUIUtilCreateTaskToken();
int APS5_VABI sceShellUIUtilGetAppUrl(const char* application, const char* parameter, char* output, std::uint32_t capacity, std::uint32_t* outputSize);
int APS5_VABI sceShellUIUtilInitialize();
int APS5_VABI sceShellUIUtilLaunchByUri(const char* uri, const ShellUIUtilLaunchParameter* parameter);
int APS5_VABI sceShellUIUtilLaunchTask(std::int32_t taskToken, const char* uri, char* output, std::uint32_t capacity);
int APS5_VABI sceShellUIUtilTerminate();

}
