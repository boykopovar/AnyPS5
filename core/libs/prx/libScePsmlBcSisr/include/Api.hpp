#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI scePsmlBcSisrGetInternalBufferRequirement(void);
int APS5_VABI scePsmlBcSisrGetPacketSizeInDwords(void);
int APS5_VABI scePsmlBcSisrInit(void);

}
