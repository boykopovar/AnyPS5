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

std::uint32_t* PushMarker(CommandBuffer* buf, const char* str, const char* function) {
    Agc::Command::Require(buf != nullptr, function, "null command buffer");
    const char* text = str ? str : "";
    const std::size_t bytes = std::strlen(text) + 1;
    const auto payload = static_cast<std::uint32_t>((bytes + 3) / 4);
    Agc::Command::Require(payload <= 0x4000u, function, "marker text too long");
    auto* packet = Agc::Command::Allocate(buf, payload + 1, function);
    packet[0] = Agc::Command::Header(OpcodeNop, payload + 1, CustomPushMarker << 2);
    std::memset(packet + 1, 0, payload * sizeof(std::uint32_t));
    std::memcpy(packet + 1, text, bytes);
    return packet;
}

std::uint32_t* PopMarker(CommandBuffer* buf, const char* function) {
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
    auto* packet = PushMarker(buf, str, __func__);
    PopMarker(buf, __func__);
    return packet;
}

uint32_t* APS5_VABI sceAgcDcbPopMarker(CommandBuffer* buf) {
    return PopMarker(buf, __func__);
}

uint32_t* APS5_VABI sceAgcDcbPushMarker(CommandBuffer* buf, const char* str, uint32_t color) {
    (void)color;
    return PushMarker(buf, str, __func__);
}

}
