#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceEsvmInitializePlayReady();
int APS5_VABI sceEsvmInitializeWidevine();
int APS5_VABI sceEsvmRequestMediaKeySystemAccessPlayReady();
int APS5_VABI sceEsvmRequestMediaKeySystemAccessWidevine();

}
