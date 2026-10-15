#ifndef CORE_LIBS_PRX_LIBSCEAGC_DCBSTATE_INCLUDE_CONTEXTSTATE_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_DCBSTATE_INCLUDE_CONTEXTSTATE_HPP

#include "SceTypes.hpp"
#include <cstdint>

struct CommandBuffer;

extern "C" {

uint32_t* APS5_VABI sceAgcDcbContextStateOp(CommandBuffer* buf, uint32_t operation);
uint32_t* APS5_VABI sceAgcDcbContextStateOp_0100(CommandBuffer* buf, uint32_t operation);
uint64_t APS5_VABI sceAgcDcbContextStateOpGetSize(uint32_t operation);

}

#endif
