#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI scePadTrackerCalibrate(void);
int APS5_VABI scePadTrackerGetWorkingMemorySize(std::uint32_t* first, std::uint32_t* second);
int APS5_VABI scePadTrackerInit(void* firstMemory, void* secondMemory, std::int32_t first, std::int32_t second);
int APS5_VABI scePadTrackerReadState(std::int32_t handle, void* state);
int APS5_VABI scePadTrackerTerm(void);
int APS5_VABI scePadTrackerUpdate();

}
