#include "prx/libSceAgc/Misc/include/Debug.hpp"

#include <atomic>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

std::atomic<std::uint32_t> shaderInstrumentation{0};

}

extern "C" {

int APS5_VABI sceAgcDebugRaiseException(std::uint32_t exceptionId) {
    APS5_LOG_OUT("sceAgcDebugRaiseException exceptionId=0x%08x", static_cast<unsigned>(exceptionId));
    return 0;
}

int APS5_VABI sceAgcSetShaderInstrumentation(std::uint32_t flags) {
    shaderInstrumentation.store(flags, std::memory_order_relaxed);
    return 0;
}

std::uint32_t APS5_VABI sceAgcGetShaderInstrumentation() {
    return shaderInstrumentation.load(std::memory_order_relaxed);
}

}
