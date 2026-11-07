#include "prx/libSceAvPlayer.native/include/Api.hpp"
#include <cstdarg>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestVaList.hpp"
#include "prx/libSceAvPlayer/include/LogCallback.hpp"

extern "C" {

int APS5_VABI sceAvPlayerPrintf(const char* format, ...) {
    if (!format) return -1;
    APS5_VA_BEGIN(format);
    const int result = SceAvPlayerWriteLog(format, reinterpret_cast<VaList*>(args));
    APS5_VA_END();
    return result;
}

int APS5_VABI sceAvPlayerVprintf(const char* format, VaList* arguments) {
    if (!format) return -1;
    return SceAvPlayerWriteLog(format, arguments);
}

}
