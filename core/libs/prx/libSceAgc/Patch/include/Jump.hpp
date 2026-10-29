#ifndef CORE_LIBS_PRX_LIBSCEAGC_PATCH_INCLUDE_JUMP_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_PATCH_INCLUDE_JUMP_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

int APS5_VABI sceAgcJumpPatchSetTarget(uint32_t* cmd, const volatile uint32_t* target, uint32_t sizeInDwords);
int APS5_VABI sceAgcJumpPatchSetTarget_0300(uint32_t* cmd, uint64_t cachePolicy, const volatile uint32_t* target, uint32_t sizeInDwords);

}

#endif
