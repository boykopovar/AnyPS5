#pragma once

#include <cstdarg>

extern "C" {

typedef int (*SceAvPlayerLogCallback)(void* user_data, const char* format, va_list arguments);
void SceAvPlayerSetLogCallbackState(void* callback, void* user_data);
int SceAvPlayerWriteLog(const char* format, va_list arguments);

}
