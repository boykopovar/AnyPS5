#include "prx/libSceAvPlayer.native/include/Api.hpp"
#include <cstdarg>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceAvPlayer/include/LogCallback.hpp"

extern "C" {

int APS5_VABI sceAvPlayerPrintf(const char* format, ...) {
    if (!format) return -1;
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, format);
    const int result = SceAvPlayerWriteLog(format, reinterpret_cast<VaList*>(arguments));
    __builtin_sysv_va_end(arguments);
#else
    std::va_list arguments;
    va_start(arguments, format);
    const int result = SceAvPlayerWriteLog(format, reinterpret_cast<VaList*>(arguments));
    va_end(arguments);
#endif
    return result;
}

int APS5_VABI sceAvPlayerVprintf(const char* format, VaList* arguments) {
    if (!format) return -1;
    return SceAvPlayerWriteLog(format, arguments);
}

}
