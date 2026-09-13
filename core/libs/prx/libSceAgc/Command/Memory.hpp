#ifndef CORE_LIBS_PRX_LIBSCEAGC_COMMAND_MEMORY_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_COMMAND_MEMORY_HPP

#include "prx/libSceAgc/Command/Packet.hpp"

namespace Agc::Command {

std::uint32_t* WriteDma(CommandBuffer* buffer, bool compute, std::uint8_t engine, std::uint8_t dst, std::uint8_t dstCachePolicy, std::uint64_t dstAddress, std::uint8_t src, std::uint8_t srcCachePolicy, std::uint64_t srcAddress, std::uint32_t numBytes, std::uint8_t waitForPrevious, std::uint8_t writeConfirm, std::uint8_t blockEngine, const char* function);
std::uint32_t* WriteData(CommandBuffer* buffer, bool compute, std::uint8_t dst, std::uint8_t cachePolicy, std::uint64_t address, const void* data, std::uint32_t count, std::uint8_t increment, std::uint8_t writeConfirm, const char* function);
std::uint32_t* WriteWait(CommandBuffer* buffer, std::uint8_t size, std::uint8_t compareFunction, std::uint8_t operation, std::uint8_t cachePolicy, const volatile void* address, std::uint64_t reference, std::uint64_t mask, std::uint32_t pollCycles, const char* function);
std::uint32_t* ValidateWait(std::uint32_t* packet, const char* function);

}

#endif
