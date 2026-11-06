#ifndef CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_REGISTERDEFAULTS_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_REGISTERDEFAULTS_HPP
#include "SceTypes.hpp"
#include <cstdint>

extern "C" {
int APS5_VABI sceAgcGetDefaultCxStateFlat(void* outState, std::uint32_t size);
}

#include "SceShaders.hpp"

extern "C" {

void* APS5_VABI sceAgcGetRegisterDefaults();
void* APS5_VABI sceAgcGetRegisterDefaultsInternal();
void* APS5_VABI sceAgcGetRegisterDefaults2(std::uint32_t version);
void* APS5_VABI sceAgcGetRegisterDefaults2Internal(std::uint32_t version);

}

#endif
