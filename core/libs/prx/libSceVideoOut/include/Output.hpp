#ifndef CORE_LIBS_PRX_LIBSCEVIDEOUOUT_INCLUDE_OUTPUT_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOUOUT_INCLUDE_OUTPUT_HPP

#include <cstdint>
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVideoOutOpen(int user_id, int bus_type, int index, const void* param);
int APS5_VABI sceVideoOutClose(int handle);
int APS5_VABI sceVideoOutConfigureOutput(int handle, uint64_t mode, const VideoOutOutputOptions* options, void* reserved_ptr, uint64_t reserved);
int APS5_VABI sceVideoOutIsOutputSupported(int handle, uint64_t mode, const VideoOutOutputOptions* options, void* reserved_ptr, uint64_t reserved);
int APS5_VABI sceVideoOutInitializeOutputOptions(VideoOutOutputOptions* options);
int APS5_VABI sceVideoOutSetFlipRate(int handle, int rate);
int APS5_VABI sceVideoOutSetWindowModeMargins(int handle, int top, int bottom);
int APS5_VABI sceVideoOutGetFlipStatus(int handle, VideoOutFlipStatus* status);
int APS5_VABI sceVideoOutGetOutputStatus(int handle, VideoOutOutputStatus* status);
int APS5_VABI sceVideoOutGetVblankStatus(int handle, VideoOutVblankStatus* status);
int APS5_VABI sceVideoOutIsFlipPending(int handle);
int APS5_VABI sceVideoOutWaitVblank(int handle);

#endif

}
