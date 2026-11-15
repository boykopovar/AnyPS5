#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <atomic>
#include <cstdint>
#include <cstring>

namespace {

std::atomic<bool> g_superSamplerEnabled{false};
std::atomic<unsigned int> g_osrExitFuzzChecks{0};
std::atomic<unsigned int> g_staticOsrExitFuzzChecks{0};
std::atomic<unsigned int> g_exceptionFuzzChecks{0};
std::atomic<unsigned int> g_executableAllocationFuzzChecks{0};

}  // namespace

extern "C" {
void APS5_VABI _ZN3JSC16clearArrayMemsetEPdj(double* base, unsigned int count) {
    const std::uint64_t pnanBits = 0x7FF8000000000000ULL;
    double pnanValue;
    std::memcpy(&pnanValue, &pnanBits, sizeof(double));
    for (unsigned int i = 0; i < count; ++i) {
        base[i] = pnanValue;
    }
}

void APS5_VABI _ZN3JSC18enableSuperSamplerEv() {
    g_superSamplerEnabled.store(true, std::memory_order_relaxed);
}

void APS5_VABI _ZN3JSC19disableSuperSamplerEv() {
    g_superSamplerEnabled.store(false, std::memory_order_relaxed);
}

void APS5_VABI _ZN3JSC22printSuperSamplerStateEv() {
}

unsigned int APS5_VABI _ZN3JSC25numberOfOSRExitFuzzChecksEv() {
    return g_osrExitFuzzChecks.load(std::memory_order_relaxed);
}

unsigned int APS5_VABI _ZN3JSC31numberOfStaticOSRExitFuzzChecksEv() {
    return g_staticOsrExitFuzzChecks.load(std::memory_order_relaxed);
}

unsigned int APS5_VABI _ZN3JSC27numberOfExceptionFuzzChecksEv() {
    return g_exceptionFuzzChecks.load(std::memory_order_relaxed);
}

unsigned int APS5_VABI _ZN3JSC38numberOfExecutableAllocationFuzzChecksEv() {
    return g_executableAllocationFuzzChecks.load(std::memory_order_relaxed);
}

void APS5_VABI _ZN3JSC27finalizeStatsAtEndOfTestingEv() {
}

void APS5_VABI _ZN3JSC20waitForVMDestructionEv() {
}

void APS5_VABI _ZN3JSC30waitForAsynchronousDisassemblyEv() {
}

void APS5_VABI _ZN3JSC18logTotalPhaseTimesEv() {
}

}
