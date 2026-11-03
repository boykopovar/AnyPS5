#ifndef CORE_LIBS_PRX_LIBSCEAGC_DCBFLOW_INCLUDE_CONTROL_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_DCBFLOW_INCLUDE_CONTROL_HPP

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {
std::uint32_t* APS5_VABI sceAgcDcbAtomicGds(CommandBuffer* buf, std::uint32_t control, std::uint32_t mode, std::uint32_t gdsOp, std::uint32_t src, std::uint16_t offset, std::uint16_t index, std::uint32_t mask, std::uint32_t data, std::uint64_t compareData, std::uint64_t extraData);
std::uint32_t* APS5_VABI sceAgcDcbAtomicGds_0900(CommandBuffer* buf, std::uint32_t control, std::uint32_t mode, std::uint32_t gdsOp, std::uint32_t src, std::uint16_t offset, std::uint16_t index, std::uint32_t mask, std::uint32_t data, std::uint64_t compareData, std::uint64_t extraData);
}

#include "SceShaders.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcDcbJump(CommandBuffer* buf, uint8_t mode, uint8_t cachePolicy, const uint32_t* target, uint32_t sizeInDwords);
std::uint32_t APS5_VABI sceAgcDcbJumpGetSize();
std::uint32_t* APS5_VABI sceAgcDcbResetQueue(CommandBuffer* buf, std::uint32_t op, std::uint32_t state);
std::uint32_t* APS5_VABI sceAgcDcbClearState(CommandBuffer* buf, std::uint32_t command);
uint32_t* APS5_VABI sceAgcDcbRewind(CommandBuffer* buf, uint32_t initialState);
uint32_t APS5_VABI sceAgcDcbRewindGetSize(void);
uint32_t* APS5_VABI sceAgcDcbWaitUntilSafeForRendering(CommandBuffer* buf, uint32_t videoOutHandle, uint32_t displayBufferIndex);

}

#endif
