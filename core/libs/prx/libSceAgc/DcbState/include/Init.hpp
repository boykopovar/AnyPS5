#ifndef CORE_LIBS_PRX_LIBSCEAGC_DCBSTATE_INCLUDE_INIT_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_DCBSTATE_INCLUDE_INIT_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

int APS5_VABI sceAgcInit(std::uint32_t version);
int APS5_VABI sceAgcInit_0090(std::uint32_t* state, std::uint32_t version);

}

#endif
