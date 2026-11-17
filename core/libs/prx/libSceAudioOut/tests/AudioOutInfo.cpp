#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>

int main() {
    Require(sceAudioOutGetSimulatedHandleStatusInfo() == static_cast<int>(0x809B00FF), "simulated handle status info is not supported");
    Require(sceAudioOutGetInfoOpenNum() == 0, "open port count info returns 0");
    Require(sceAudioOutGetInfo() == 0, "port info returns 0");
    std::puts("audio out info calls: PASS");
}
