#include "prx/libSceAgcDriver/include/AgcDriver.hpp"
#include "SceTypes.hpp"

int main() {
    if (sceAgcDriverGetRegShadowInfo(nullptr) != static_cast<int>(0x8A6D0003)) return 1;
    if (sceAgcDriverGetRegShadowInfoAgr(nullptr) != static_cast<int>(0x8A6D0003)) return 2;
    return 0;
}
