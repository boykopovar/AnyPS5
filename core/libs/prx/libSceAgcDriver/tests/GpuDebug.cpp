#include "prx/libSceAgcDriver/include/AgcDriver.hpp"
#include <cstdint>
#include <cstdio>

int main() {
    bool passed = true;
    if (sceAgdGetSystemGpuDebug() != 0) {
        std::printf("gpu debug: system mask is not zero\n");
        passed = false;
    }
    if (sceAgdGetApplicationGpuDebug() != 0) {
        std::printf("gpu debug: application value is not zero\n");
        passed = false;
    }
    if (sceAgcDriverIsEsvmProcess() != 0) {
        std::printf("gpu debug: esvm process is not zero\n");
        passed = false;
    }
    if (sceAgdSetApplicationGpuDebug(0) != 0) {
        std::printf("gpu debug: setting the empty value was rejected\n");
        passed = false;
    }
    if (sceAgdSetApplicationGpuDebug(1) != static_cast<int>(0x8a6c0047u)) {
        std::printf("gpu debug: a bit outside the system mask was not rejected\n");
        passed = false;
    }
    if (sceAgdGetApplicationGpuDebug() != 0) {
        std::printf("gpu debug: a rejected value reached the state\n");
        passed = false;
    }
    std::printf("gpu debug state: %s\n", passed ? "passed" : "failed");
    return passed ? 0 : 1;
}
