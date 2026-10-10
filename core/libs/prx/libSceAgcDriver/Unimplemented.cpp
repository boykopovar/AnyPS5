#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcDriverGetShaderDebuggingStatus() {
 throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceAgcDriverRegisterMultipleResources() {
 throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceAgcDriverRegisterDefaultOwner(uint32_t* owner_handle) {
    (void)owner_handle;
    return static_cast<int>(0x8A6C9018);
}

int APS5_VABI sceAgcDriverSetValidationErrorOutputFrequency() {
 throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceAgcDriverSetSubmitValidationMode() {
 throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceAgcDriverGetSubmitValidationConfig() {
 throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceAgcDriverGetSubmitValidationMode() {
 throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceAgcDriverSetSubmitValidationConfig() {
 throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

}

