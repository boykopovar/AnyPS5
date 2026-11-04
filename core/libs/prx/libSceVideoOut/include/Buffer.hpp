#ifndef CORE_LIBS_PRX_LIBSCEVIDEOUOUT_INCLUDE_BUFFER_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOUOUT_INCLUDE_BUFFER_HPP

#include <cstdint>

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVideoOutRegisterBuffers(int handle, int bufferIndexStart, void* const* addresses, int bufferNum, const VideoOutBufferAttribute* attribute);
void APS5_VABI sceVideoOutSetBufferAttribute(VideoOutBufferAttribute* attribute, uint32_t pixelFormat, uint32_t tilingMode, uint32_t aspectRatio, uint32_t width, uint32_t height, uint32_t pitchInPixel);
int APS5_VABI sceVideoOutRegisterBuffers2(int handle, int setIndex, int bufferIndexStart, const VideoOutBuffers* buffers, int bufferNum, const VideoOutBufferAttribute2* attribute, int category, void* option);
void APS5_VABI sceVideoOutSetBufferAttribute2(VideoOutBufferAttribute2* attribute, uint64_t pixelFormat, uint32_t tilingMode, uint32_t width, uint32_t height, uint64_t option, uint32_t dccControl, uint64_t dccCbRegisterClearColor);
void APS5_VABI sceVideoOutAddBufferAttributeOption(VideoOutBufferAttribute2* attribute, uint64_t option);
void APS5_VABI sceVideoOutRemoveBufferAttributeOption(VideoOutBufferAttribute2* attribute, uint64_t option);
int APS5_VABI sceVideoOutSubmitChangeBufferAttribute2(int handle, int setIndex, const VideoOutBufferAttribute2* attribute, void* option);
int APS5_VABI sceVideoOutSubmitEopFlip(int handle, int index, int flipMode, int64_t flipArg, std::uint32_t* token);
int APS5_VABI sceVideoOutSubmitFlip(int handle, int index, int flipMode, int64_t flipArg);
int APS5_VABI sceVideoOutUnregisterBuffers(int handle, int setIndex);
int APS5_VABI sceVideoOutGetBufferLabelAddress(int handle, uintptr_t* address);

}

#endif
