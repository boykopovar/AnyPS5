#include "prx/libSceAgc/Misc/include/Debug.hpp"

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

std::atomic<std::uint32_t> shaderInstrumentationFlags{0};

}

extern "C" {

int APS5_VABI sceAgcDebugRaiseException(std::uint32_t exceptionId) {
    APS5_LOG_OUT("sceAgcDebugRaiseException exceptionId=0x%08x", static_cast<unsigned>(exceptionId));
    return 0;
}

std::uint32_t APS5_VABI sceAgcGetShaderInstrumentation() {
    return shaderInstrumentationFlags.load(std::memory_order_relaxed);
}

int APS5_VABI sceAgcSetShaderInstrumentation(std::uint32_t flags) {
    if (flags != 0) throw std::invalid_argument("sceAgcSetShaderInstrumentation: nonzero flags are unsupported");
    shaderInstrumentationFlags.store(flags, std::memory_order_relaxed);
    return 0;
}

}
