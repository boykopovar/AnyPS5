#include "prx/libSceAjm.native/include/Api.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"

namespace {

constexpr int SCE_AJM_ERROR_INVALID_PARAMETER = static_cast<int>(0x80930005);
constexpr int SCE_AJM_ERROR_INVALID_FLAGS = static_cast<int>(0x8093000C);
constexpr std::uint64_t ReservedFlags = 0xf0000000ffffc000ull;

[[noreturn]] void AjmDriver(const char* function) {
    throw std::runtime_error(std::string(function) + ": the request to the AJM driver");
}

}

extern "C" {

int APS5_VABI sceAjmInstanceGetSize(uint32_t, uint32_t, uint64_t flags) {
    if ((flags & 0x380u) >= 0x180u || (flags & 0x2400u) == 0x2000u || (flags & ReservedFlags) != 0) return SCE_AJM_ERROR_INVALID_FLAGS;
    AjmDriver(__func__);
}

int APS5_VABI sceAjmGetFailedInstance(uint32_t, void* failed) {
    if (!failed) return SCE_AJM_ERROR_INVALID_PARAMETER;
    AjmDriver(__func__);
}

int APS5_VABI sceAjmTelemetryGetLog(uint32_t, uint8_t*, void* log) {
    if (!log) return SCE_AJM_ERROR_INVALID_PARAMETER;
    AjmDriver(__func__);
}

}
