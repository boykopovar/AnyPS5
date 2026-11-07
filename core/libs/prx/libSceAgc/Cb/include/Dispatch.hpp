#ifndef CORE_LIBS_PRX_LIBSCEAGC_CB_INCLUDE_DISPATCH_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_CB_INCLUDE_DISPATCH_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcCbDispatch(CommandBuffer* buf, std::uint32_t threadGroupX, std::uint32_t threadGroupY, std::uint32_t threadGroupZ, std::uint32_t modifier);
uint32_t APS5_VABI sceAgcCbDispatchGetSize(void);

}

#endif
