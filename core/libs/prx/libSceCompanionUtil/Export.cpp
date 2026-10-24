#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceCompanionUtilGetEvent() {
    return static_cast<int>(0x80AD0008);
}

int APS5_VABI sceCompanionUtilGetRemoteOskEvent() {
    return 0;
}

int APS5_VABI sceCompanionUtilInitialize() {
    return 0;
}

int APS5_VABI sceCompanionUtilOptParamInitialize() {
    return 0;
}

int APS5_VABI sceCompanionUtilTerminate() {
    return 0;
}

}
