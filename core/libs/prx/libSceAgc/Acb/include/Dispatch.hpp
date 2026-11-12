#ifndef CORE_LIBS_PRX_LIBSCEAGC_ACB_INCLUDE_DISPATCH_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_ACB_INCLUDE_DISPATCH_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcAcbDispatchIndirect(CommandBuffer* buf, const volatile void* indirectArgs, uint32_t modifier);
std::uint32_t APS5_VABI sceAgcAcbDispatchIndirectGetSize();

}

#endif
