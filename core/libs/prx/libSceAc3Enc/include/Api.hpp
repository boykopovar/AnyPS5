#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAc3EncCreateEncoder();
int APS5_VABI sceAc3EncDeleteEncoder();
int APS5_VABI sceAc3EncEncode();
int APS5_VABI sceAc3EncEncodeAsync();
int APS5_VABI sceAc3EncFlush();
int APS5_VABI sceAc3EncIsEncoding();
int APS5_VABI sceAc3EncWaitEncode();

}
