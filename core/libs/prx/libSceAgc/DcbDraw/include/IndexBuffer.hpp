#ifndef CORE_LIBS_PRX_LIBSCEAGC_DCBDRAW_INCLUDE_INDEXBUFFER_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_DCBDRAW_INCLUDE_INDEXBUFFER_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcDcbSetIndexBuffer(CommandBuffer* buf, std::uint64_t indexAddress);
std::uint32_t APS5_VABI sceAgcDcbSetIndexBufferGetSize();
std::uint32_t* APS5_VABI sceAgcDcbSetIndexCount(CommandBuffer* buf, std::uint32_t indexCount);
std::uint32_t APS5_VABI sceAgcDcbSetIndexCountGetSize();
std::uint32_t* APS5_VABI sceAgcDcbSetIndexSize(CommandBuffer* buf, std::uint8_t indexSize, std::uint8_t cachePolicy);
std::uint32_t APS5_VABI sceAgcDcbSetIndexSizeGetSize();
std::uint32_t* APS5_VABI sceAgcDcbSetIndexSize_0600(CommandBuffer* buf, std::uint8_t indexSize, std::uint8_t cachePolicy, std::uint8_t perInstanceObjectId);
std::uint32_t* APS5_VABI sceAgcDcbSetIndexIndirectArgs(CommandBuffer* buf, std::uint64_t attributeAddress, std::uint32_t attributeIndex);
std::uint32_t APS5_VABI sceAgcDcbSetIndexIndirectArgsGetSize();

}

#endif
