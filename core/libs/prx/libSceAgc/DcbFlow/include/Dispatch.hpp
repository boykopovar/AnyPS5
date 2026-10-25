#ifndef CORE_LIBS_PRX_LIBSCEAGC_DCBFLOW_INCLUDE_DISPATCH_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_DCBFLOW_INCLUDE_DISPATCH_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcDcbDispatchIndirect(CommandBuffer* buf, uint32_t dataOffsetInBytes, uint32_t flags);
std::uint32_t APS5_VABI sceAgcDcbDispatchIndirectGetSize();

}

#endif
