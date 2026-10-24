#ifndef CORE_LIBS_PRX_LIBSCEAGC_CB_INCLUDE_SYNC_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_CB_INCLUDE_SYNC_HPP

#include "SceTypes.hpp"
#include <cstdint>

struct CommandBuffer;

extern "C" {
std::uint32_t* APS5_VABI sceAgcCbMemSemaphore(CommandBuffer* buf, const volatile void* address, std::uint32_t operation, std::uint32_t modifier, std::uint32_t value);
}

#endif
