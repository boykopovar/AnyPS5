#pragma once

#include "SceTypes.hpp"
#include <cstdarg>

extern "C" {

typedef int (APS5_VABI* SceAvPlayerLogCallback)(void* user_data, const char* format, VaList* arguments);
void SceAvPlayerSetLogCallbackState(void* callback, void* user_data);
int SceAvPlayerWriteLog(const char* format, VaList* arguments);

}
