#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcAcbPushMarkerSpan(CommandBuffer* buffer, const char* text, std::uint32_t length, std::uint32_t color) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::uint32_t* APS5_VABI sceAgcAcbSetMarkerSpan(CommandBuffer* buffer, const char* text, std::uint32_t length, std::uint32_t color) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcAcquireMemSetEngine(std::uint32_t* packet, std::uint8_t engine) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::uint32_t* APS5_VABI sceAgcDcbPushMarkerSpan(CommandBuffer* buffer, const char* text, std::uint32_t length, std::uint32_t color) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::uint32_t* APS5_VABI sceAgcDcbSetMarkerSpan(CommandBuffer* buffer, const char* text, std::uint32_t length, std::uint32_t color) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcSetSemaphoreMemory(void* memory, std::uint64_t size) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::uint32_t* APS5_VABI sceAgcCbMemsetExclusive(CommandBuffer* buffer, void* destination, const void* pattern, std::uint64_t size) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
