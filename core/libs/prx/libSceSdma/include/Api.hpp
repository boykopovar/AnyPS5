#pragma once

#include "SceTypes.hpp"

struct SdmaOpenParam {
    std::uint32_t engine;
    std::uint32_t reserved;
    std::uint64_t fenceAddress;
    std::uint64_t writeBackAddress;
    std::uint64_t writeBackExtra;
    std::uint32_t flags;
};

extern "C" {

int APS5_VABI sceSdmaBeginQueueMode(void* handle);
int APS5_VABI sceSdmaCancelQueue(void* handle);
int APS5_VABI sceSdmaClose(void* handle);
int APS5_VABI sceSdmaConstantFill(void* handle, std::uint32_t value, std::uint64_t destination, std::uint32_t bytes, std::uint32_t mode);
int APS5_VABI sceSdmaConstantFillNonBlocking(void* handle, std::uint32_t value, std::uint64_t destination, std::uint32_t bytes, std::uint32_t mode);
int APS5_VABI sceSdmaCopyLinear(void* handle, std::uint64_t source, std::uint64_t destination, std::uint32_t bytes);
int APS5_VABI sceSdmaCopyLinearNonBlocking(void* handle, std::uint64_t source, std::uint64_t destination, std::uint32_t bytes);
int APS5_VABI sceSdmaCopyTiled(void);
int APS5_VABI sceSdmaCopyTiledNonBlocking(void);
int APS5_VABI sceSdmaCopyWindowL2L(void* handle, std::uint64_t source, std::uint64_t destination, std::uint32_t bitsPerElement, std::uint32_t width,
                                   std::uint32_t height, std::uint32_t sourcePitch, std::uint32_t sourceHeight, std::uint32_t sourceX,
                                   std::uint32_t sourceY, std::uint32_t destinationPitch, std::uint32_t destinationHeight,
                                   std::uint32_t destinationX, std::uint32_t destinationY);
int APS5_VABI sceSdmaCopyWindowL2LNonBlocking(void* handle, std::uint64_t source, std::uint64_t destination, std::uint32_t bitsPerElement,
                                              std::uint32_t width, std::uint32_t height, std::uint32_t sourcePitch, std::uint32_t sourceHeight,
                                              std::uint32_t sourceX, std::uint32_t sourceY, std::uint32_t destinationPitch,
                                              std::uint32_t destinationHeight, std::uint32_t destinationX, std::uint32_t destinationY);
int APS5_VABI sceSdmaCopyWindowT2T(void);
int APS5_VABI sceSdmaCopyWindowT2TNonBlocking(void);
int APS5_VABI sceSdmaCopyWindowTiled(void);
int APS5_VABI sceSdmaCopyWindowTiledNonBlocking(void);
int APS5_VABI sceSdmaEndQueueMode(void* handle);
int APS5_VABI sceSdmaFenceNonBlocking(void* handle);
int APS5_VABI sceSdmaFinalize(void);
int APS5_VABI sceSdmaFlushAll(void* handle);
int APS5_VABI sceSdmaFlushAllWithEvent(void);
int APS5_VABI sceSdmaInitialize(void);
int APS5_VABI sceSdmaKickQueue(void* handle);
int APS5_VABI sceSdmaOpen(void** handle, void* buffer, std::uint32_t size, const SdmaOpenParam* param);
int APS5_VABI sceSdmaQueryWorkingBufferSize(void);
int APS5_VABI sceSdmaTryFlush(void* handle);
int APS5_VABI sceSdmaWaitForEvent(void);

}
