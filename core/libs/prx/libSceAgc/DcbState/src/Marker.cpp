#include "prx/libSceAgc/DcbState/include/Marker.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// Debug markers are custom NOP packets: PUSH carries the NUL-terminated text, POP has no payload.
// Marker colors are only meaningful to capture tools and are not encoded.
namespace {

constexpr std::uint32_t OpcodeNop = 0x10;
constexpr std::uint32_t CustomPushMarker = 0x0b;
constexpr std::uint32_t CustomPopMarker = 0x0c;

}

namespace Agc::Marker {

std::uint32_t* Push(CommandBuffer* buf, const char* str, const char* function) {
    return Push(buf, str, str ? std::strlen(str) : 0, function);
}

std::uint32_t* Push(CommandBuffer* buf, const char* str, std::size_t length, const char* function) {
    Agc::Command::Require(buf != nullptr, function, "null command buffer");
    Agc::Command::Require(str != nullptr || length == 0, function, "null marker text");
    Agc::Command::Require(length < 0x10000u, function, "marker text too long");
    const auto payload = static_cast<std::uint32_t>((length + 4) / 4);
    auto* packet = Agc::Command::Allocate(buf, payload + 1, function);
    packet[0] = Agc::Command::Header(OpcodeNop, payload + 1, CustomPushMarker << 2);
    std::memset(packet + 1, 0, payload * sizeof(std::uint32_t));
    if (length != 0) std::memcpy(packet + 1, str, length);
    return packet;
}

std::uint32_t* Pop(CommandBuffer* buf, const char* function) {
    Agc::Command::Require(buf != nullptr, function, "null command buffer");
    auto* packet = Agc::Command::Allocate(buf, 2, function);
    packet[0] = Agc::Command::Header(OpcodeNop, 2, CustomPopMarker << 2);
    packet[1] = 0;
    return packet;
}

}

extern "C" {

uint32_t* APS5_VABI sceAgcDcbSetMarker(CommandBuffer* buf, const char* str, uint32_t color) {
    (void)color;
    auto* packet = Agc::Marker::Push(buf, str, __func__);
    Agc::Marker::Pop(buf, __func__);
    return packet;
}

uint32_t* APS5_VABI sceAgcDcbPopMarker(CommandBuffer* buf) {
    return Agc::Marker::Pop(buf, __func__);
}

uint32_t* APS5_VABI sceAgcDcbPushMarker(CommandBuffer* buf, const char* str, uint32_t color) {
    (void)color;
    return Agc::Marker::Push(buf, str, __func__);
}

uint32_t* APS5_VABI sceAgcDcbSetMarkerSpan(CommandBuffer* buf, const char* str, uint32_t length, uint32_t color) {
    (void)color;
    auto* packet = Agc::Marker::Push(buf, str, length, __func__);
    Agc::Marker::Pop(buf, __func__);
    return packet;
}

uint32_t* APS5_VABI sceAgcDcbPushMarkerSpan(CommandBuffer* buf, const char* str, uint32_t length, uint32_t color) {
    (void)color;
    return Agc::Marker::Push(buf, str, length, __func__);
}

}
