#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceDeci5Write(void);
int APS5_VABI sceDeci5Read(void);
int APS5_VABI sceDeci5CreateHostProcessAndWait(void);
int APS5_VABI sceDeci5Close(void);
int APS5_VABI sceDeci5CreateHostProcess(void);
int APS5_VABI sceDeci5Open(void);
int APS5_VABI sceDeci5GetStatus(void);

}
