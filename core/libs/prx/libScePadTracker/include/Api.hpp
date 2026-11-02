#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI scePadTrackerCalibrate();
int APS5_VABI scePadTrackerGetWorkingMemorySize(std::uint32_t* first, std::uint32_t* second);
int APS5_VABI scePadTrackerInit();
int APS5_VABI scePadTrackerReadState();
int APS5_VABI scePadTrackerTerm();
int APS5_VABI scePadTrackerUpdate();

}
