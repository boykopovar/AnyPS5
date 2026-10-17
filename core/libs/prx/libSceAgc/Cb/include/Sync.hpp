#ifndef CORE_LIBS_PRX_LIBSCEAGC_CB_INCLUDE_SYNC_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_CB_INCLUDE_SYNC_HPP

#include "SceTypes.hpp"
#include <cstdint>

struct CommandBuffer;

extern "C" {
std::uint32_t* APS5_VABI sceAgcCbMemSemaphore(CommandBuffer* buf, std::uint8_t action, std::uint8_t clientCode, const volatile void* address);
}

#endif
