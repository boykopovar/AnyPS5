#pragma once

#include "SceTypes.hpp"

struct SceSystemLogger2Timestamp {
    std::uint64_t tick;
    char clockSource[24];
};

extern "C" {

int APS5_VABI sceSystemLogger2Initialize(void);
int APS5_VABI sceSystemLogger2Terminate(void);
int APS5_VABI sceSystemLogger2TerminateWithResourceRelease(void);
int APS5_VABI sceSystemLogger2GetTimestamp(SceSystemLogger2Timestamp* timestamp);
int APS5_VABI sceSystemLogger2IsAvailableMonitor(void);
int APS5_VABI sceSystemLogger2GetEventStatus(const char* eventName, std::uint64_t* status);
int APS5_VABI sceSystemLogger2SetEventAsync(const void* event);
int APS5_VABI sceSystemLogger2SetEventAsyncEx(const void* event);
int APS5_VABI sceSystemLogger2SetEventAsyncEx2(const void* event);
int APS5_VABI sceSystemLogger2SetRnpsEventAsync(const void* event);
int APS5_VABI sceSystemLogger2SetEventAsyncInternal(const void* event, std::uint32_t* status);

}
