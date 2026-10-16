#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int DEBUG_UNAVAILABLE = static_cast<int>(0x8A6C1000);

}

extern "C" {

int APS5_VABI sceAgcDriverGetShaderDebuggingStatus() {
    return 1;
}

int APS5_VABI sceAgcDriverRegisterDefaultOwner(uint32_t owner) {
    (void)owner;
    return 0;
}

int APS5_VABI sceAgcDriverSetValidationErrorOutputFrequency(uint32_t frequency) {
    (void)frequency;
    return DEBUG_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverSetSubmitValidationMode(uint32_t mode) {
    (void)mode;
    return DEBUG_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverGetSubmitValidationConfig(void* config) {
    (void)config;
    return DEBUG_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverGetSubmitValidationMode(uint32_t* mode) {
    (void)mode;
    return DEBUG_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverSetSubmitValidationConfig(const void* config) {
    (void)config;
    return DEBUG_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverGetHsOffchipParam(uint16_t* first, uint16_t* second) {
    (void)first;
    (void)second;
    return DEBUG_UNAVAILABLE;
}

}
