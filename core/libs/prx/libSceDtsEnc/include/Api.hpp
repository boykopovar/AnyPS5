#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceDtsEncCreateEncoder();
int APS5_VABI sceDtsEncDeleteEncoder();
int APS5_VABI sceDtsEncEncode();
int APS5_VABI sceDtsEncEncodeAsync();
int APS5_VABI sceDtsEncFlush();
int APS5_VABI sceDtsEncIsEncoding();
int APS5_VABI sceDtsEncWaitEncode();

}
