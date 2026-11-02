#include "prx/libSceAgcDriver/include/AgcDriver.hpp"
#include "SceTypes.hpp"
#include <stdexcept>

static bool Throws(int (APS5_VABI* function)(void*)) {
    int info[16] = {};
    try {
        function(info);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    if (sceAgcDriverGetRegShadowInfo(nullptr) != static_cast<int>(0x8A6D0003)) return 1;
    if (sceAgcDriverGetRegShadowInfoAgr(nullptr) != static_cast<int>(0x8A6D0003)) return 2;
    if (!Throws(&sceAgcDriverGetRegShadowInfo)) return 3;
    if (!Throws(&sceAgcDriverGetRegShadowInfoAgr)) return 4;
    return 0;
}
