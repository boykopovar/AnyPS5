#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int32_t APS5_VABI sceDtsEncCreateEncoder(int32_t* handle, const void* param, int32_t* status);
int APS5_VABI sceDtsEncDeleteEncoder();
int APS5_VABI sceDtsEncEncode();
int APS5_VABI sceDtsEncEncodeAsync();
int APS5_VABI sceDtsEncFlush();
int APS5_VABI sceDtsEncIsEncoding();
int APS5_VABI sceDtsEncWaitEncode();

}
