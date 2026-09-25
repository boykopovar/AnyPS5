#include "prx/libSceAgc/Acb/include/Control.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

// unknown signature
APS5_EXPORT("gQkqkLttcpw", sceAgcAcb_gQkqkLttcpw);
void* APS5_VABI sceAgcAcb_gQkqkLttcpw (void) {
    NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

std::uint32_t* APS5_VABI sceAgcAcbJump(CommandBuffer* buf, std::uint8_t cachePolicy, const std::uint32_t* target, std::uint32_t sizeInDwords) {
    (void)buf;
    (void)cachePolicy;
    (void)target;
    (void)sizeInDwords;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

uint32_t APS5_VABI sceAgcAcbJumpGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

// Encoded as the custom DISPATCH_RESET NOP packet the driver consumes on compute queues.
uint32_t* APS5_VABI sceAgcAcbResetQueue(CommandBuffer* buf, uint32_t op, uint32_t value) {
    (void)value;
    Agc::Command::Require(buf != nullptr, __func__, "null command buffer");
    constexpr std::uint32_t OpcodeNop = 0x10;
    constexpr std::uint32_t CustomDispatchReset = 0x09;
    auto* packet = Agc::Command::Allocate(buf, 2, __func__);
    packet[0] = Agc::Command::Header(OpcodeNop, 2, CustomDispatchReset << 2);
    packet[1] = op;
    return packet;
}

std::uint32_t* APS5_VABI sceAgcAcbRewind(CommandBuffer* buf, std::uint32_t initialState) {
    (void)buf;
    (void)initialState;
    NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

std::uint32_t APS5_VABI sceAgcAcbRewindGetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint32_t* APS5_VABI sceAgcAcbWaitUntilSafeForRendering(CommandBuffer* buf, std::uint32_t videoOutHandle, std::uint32_t displayBufferIndex) {
    (void)buf;
    (void)videoOutHandle;
    (void)displayBufferIndex;
    NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

std::uint32_t* APS5_VABI sceAgcAcbSetFlip(CommandBuffer* buf, std::uint32_t videoOutHandle, std::int32_t displayBufferIndex, std::uint32_t flipMode, std::int64_t flipArg) {
    (void)buf;
    (void)videoOutHandle;
    (void)displayBufferIndex;
    (void)flipMode;
    (void)flipArg;
    NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

uint32_t* APS5_VABI sceAgcAcbPushMarker(CommandBuffer* buf, const char* str, uint32_t color) {
 (void)buf;
 (void)str;
 (void)color;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcAcbPopMarker(CommandBuffer* buf) {
 (void)buf;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcAcbSetMarker(CommandBuffer* buf, const char* str, uint32_t color) {
 (void)buf;
 (void)str;
 (void)color;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

}
