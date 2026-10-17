#ifndef CORE_LIBS_PRX_LIBSCEAGC_PATCH_INCLUDE_WAITREGMEM_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_PATCH_INCLUDE_WAITREGMEM_HPP

#include "SceTypes.hpp"
#include <cstdint>

extern "C" int APS5_VABI sceAgcAcquireMemSetEngine(std::uint32_t* cmd, std::uint32_t engine);

#endif

