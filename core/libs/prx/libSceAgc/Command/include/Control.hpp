#ifndef CORE_LIBS_PRX_LIBSCEAGC_COMMAND_INCLUDE_CONTROL_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_COMMAND_INCLUDE_CONTROL_HPP

#include "prx/libSceAgc/Command/include/Packet.hpp"

namespace Agc::Command {

std::uint32_t* WriteJump(CommandBuffer* buffer, std::uint8_t chain, std::uint8_t cachePolicy, const std::uint32_t* target, std::uint32_t sizeInDwords, const char* function);
std::uint32_t* WriteRewind(CommandBuffer* buffer, std::uint32_t initialState, const char* function);
std::uint32_t* WritePredication(CommandBuffer* buffer, std::uint8_t condition, std::uint8_t operation, std::uint8_t waitOperation, const volatile void* address, const char* function);
// Signature corrected to (address, waitForMailbox, signalType, operation) to match the guest-facing
// sceAgc*MemSemaphore entry points; an earlier revision of this helper took a different argument order.
std::uint32_t* WriteMemSemaphore(CommandBuffer* buffer, const volatile void* address, std::uint8_t waitForMailbox, std::uint8_t signalType, std::uint8_t operation, const char* function);

}

#endif
