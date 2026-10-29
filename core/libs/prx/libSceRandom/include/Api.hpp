#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceRandomGetRandomNumber(void* buf, size_t size);

}
