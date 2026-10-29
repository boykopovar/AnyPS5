#ifndef CORE_LIBS_PRX_LIBSCEAGC_PATCH_INCLUDE_NOP_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_PATCH_INCLUDE_NOP_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcSetNop(CommandBuffer* buf, std::uint32_t sizeDw);

}

#endif
