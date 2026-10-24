#ifndef CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_REGISTERDEFAULTS_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_REGISTERDEFAULTS_HPP
#include "SceTypes.hpp"
#include <cstdint>

extern "C" {
int APS5_VABI sceAgcGetDefaultCxStateFlat(void* out_state, std::uint32_t size);
}
#endif
