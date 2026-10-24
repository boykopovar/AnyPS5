#ifndef CORE_LIBS_PRX_LIBSCEAGC_PATCH_INCLUDE_REWIND_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_PATCH_INCLUDE_REWIND_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

int APS5_VABI sceAgcRewindPatchSetRewindState(uint32_t* cmd, uint8_t state);
int APS5_VABI sceAgcAsyncRewindPatchSetRewindState(std::uint32_t* cmd, std::uint8_t state);

}

#endif
