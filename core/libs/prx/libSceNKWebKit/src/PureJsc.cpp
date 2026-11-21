#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>

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
    double chunk[512];
    std::fill(std::begin(chunk), std::end(chunk), pnanValue);
    for (std::size_t offset = 0; offset < count; offset += std::size(chunk)) {
        const std::size_t length = std::min<std::size_t>(std::size(chunk), count - offset);
        if (!GuestCopyOut_nid_no_patch(base + offset, chunk, length * sizeof(double))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
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
