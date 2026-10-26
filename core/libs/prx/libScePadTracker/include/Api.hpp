#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI scePadTrackerCalibrate();
int APS5_VABI scePadTrackerGetWorkingMemorySize();
int APS5_VABI scePadTrackerInit();
int APS5_VABI scePadTrackerReadState();
int APS5_VABI scePadTrackerTerm();
int APS5_VABI scePadTrackerUpdate();

}
