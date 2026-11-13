#ifndef CORE_LIBS_PRX_LIBSCEAGC_ACB_INCLUDE_CONTROL_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_ACB_INCLUDE_CONTROL_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcAcbAtomicGds_0900(CommandBuffer* buf, std::uint32_t control, std::uint32_t mode, std::uint32_t gdsOp, std::uint32_t src, std::uint16_t offset, std::uint16_t index, std::uint32_t mask, std::uint32_t data, std::uint64_t compareData, std::uint64_t extraData);
std::uint32_t* APS5_VABI sceAgcAcbAtomicGds(CommandBuffer* buf, std::uint32_t control, std::uint32_t mode, std::uint32_t gdsOp, std::uint32_t src, std::uint16_t offset, std::uint16_t index, std::uint32_t mask, std::uint32_t data, std::uint64_t compareData, std::uint64_t extraData);
std::uint32_t* APS5_VABI sceAgcAcbJump(CommandBuffer* buf, std::uint8_t cachePolicy, const std::uint32_t* target, std::uint32_t sizeInDwords);
uint32_t APS5_VABI sceAgcAcbJumpGetSize(void);
uint32_t* APS5_VABI sceAgcAcbResetQueue(CommandBuffer* buf, uint32_t op, uint32_t value);
std::uint32_t* APS5_VABI sceAgcAcbRewind(CommandBuffer* buf, std::uint32_t initialState);
std::uint32_t APS5_VABI sceAgcAcbRewindGetSize();
std::uint32_t* APS5_VABI sceAgcAcbWaitUntilSafeForRendering(CommandBuffer* buf, std::uint32_t videoOutHandle, std::uint32_t displayBufferIndex);
std::uint32_t* APS5_VABI sceAgcAcbSetFlip(CommandBuffer* buf, std::uint32_t videoOutHandle, std::int32_t displayBufferIndex, std::uint32_t flipMode, std::int64_t flipArg);
std::uint32_t* APS5_VABI sceAgcAcbSetWorkloadsActive(CommandBuffer* buf, std::uint32_t streamId, const std::uint32_t* workloadIds, std::uint32_t workloadCount);
std::uint32_t* APS5_VABI sceAgcAcbSetWorkloadComplete(CommandBuffer* buf, std::uint32_t streamId, std::uint32_t workloadId);
std::uint32_t* APS5_VABI sceAgcAcbSetWorkloadStreamInactive(CommandBuffer* buf, std::uint32_t streamId);
uint32_t* APS5_VABI sceAgcAcbPushMarker(CommandBuffer* buf, const char* str, uint32_t color);
uint32_t* APS5_VABI sceAgcAcbPopMarker(CommandBuffer* buf);
uint32_t* APS5_VABI sceAgcAcbSetMarker(CommandBuffer* buf, const char* str, uint32_t color);

}

#endif
