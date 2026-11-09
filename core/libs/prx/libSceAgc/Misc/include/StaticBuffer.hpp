#ifndef CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_STATICBUFFER_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_STATICBUFFER_HPP

#include <cstdint>
#include "SceTypes.hpp"

extern "C" {
int APS5_VABI sceAgcSetStaticBuffer(std::uint32_t index, const void* buffer);
int APS5_VABI sceAgcGetStaticBuffer(std::uint32_t index, void* buffer);
int APS5_VABI sceAgcSetStaticQword(std::uint32_t index, std::uint64_t value);
int APS5_VABI sceAgcGetStaticQword(std::uint32_t index, std::uint64_t* value);
}

#endif
