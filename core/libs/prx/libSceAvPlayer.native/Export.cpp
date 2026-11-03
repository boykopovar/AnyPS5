#include "prx/libSceAvPlayer.native/include/Api.hpp"
#include <cstdarg>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAvPlayerPrintf(const char* format, ...) {
    if (!format) return -1;
    va_list arguments;
    va_start(arguments, format);
    const int result = std::vprintf(format, arguments);
    va_end(arguments);
    return result;
}

int APS5_VABI sceAvPlayerVprintf(const char* format, va_list arguments) {
    if (!format) return -1;
    return std::vprintf(format, arguments);
}

}
