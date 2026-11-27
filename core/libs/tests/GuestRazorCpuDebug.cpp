#include "prx/libSceRazorCpu_debug/include/Api.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

}

int main() {
    constexpr int notInitialized = static_cast<int>(0x80580000);
    Require(sceRazorCpuStartCapture() == notInitialized);
    Require(sceRazorCpuStopCapture() == notInitialized);
    Require(sceRazorCpuSetLiveServerThreadSettings() == notInitialized);
    return 0;
}
