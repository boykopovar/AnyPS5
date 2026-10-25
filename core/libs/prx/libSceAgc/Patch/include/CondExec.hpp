#ifndef CORE_LIBS_PRX_LIBSCEAGC_PATCH_INCLUDE_CONDEXEC_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_PATCH_INCLUDE_CONDEXEC_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

int APS5_VABI sceAgcCondExecPatchSetCommandAddress(uint32_t* cmd, const volatile uint32_t* command);
int APS5_VABI sceAgcCondExecPatchSetEnd(uint32_t* cmd, const volatile uint32_t* buffer);
int APS5_VABI sceAgcAsyncCondExecPatchSetCommandAddress(std::uint32_t* cmd, const volatile std::uint32_t* command);
int APS5_VABI sceAgcAsyncCondExecPatchSetEnd(std::uint32_t* cmd, const volatile std::uint32_t* buffer);

}

#endif
