#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstdint>
#include <stdexcept>

extern "C" {
std::uint32_t* APS5_VABI sceAgcDcbSetPredication(CommandBuffer*, std::uint8_t, std::uint8_t, std::uint8_t, const volatile void*, std::uint32_t);
std::uint32_t APS5_VABI sceAgcDcbSetZPassPredicationEnableGetSize();
std::uint32_t APS5_VABI sceAgcDcbSetPredicationDisableGetSize();
std::uint32_t APS5_VABI sceAgcDcbSetBoolPredicationEnableGetSize();
std::uint32_t* APS5_VABI sceAgcDcbSetIndexCount(CommandBuffer*, std::uint32_t);
std::uint32_t* APS5_VABI sceAgcDcbSetIndexBuffer(CommandBuffer*, std::uint64_t);
int APS5_VABI sceAgcSetPacketPredication(std::uint32_t*, std::uint32_t);
int APS5_VABI sceAgcSetRangePredication(std::uint32_t*, const volatile std::uint32_t*, std::uint32_t);
}

namespace {

using Testing::Require;

template<typename TAction>
void ExpectFailure(TAction action) {
    Testing::RequireThrows<std::runtime_error>(action, "expected invalid input to fail");
}

struct Storage {
    std::array<std::uint32_t, 32> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
};

void VerifySizes() {
    Storage storage;
    alignas(16) static std::uint64_t query = 0;
    auto* packet = sceAgcDcbSetPredication(&storage.buffer, 1, 1, 0, &query, 0);
    const auto bytes = static_cast<std::uint32_t>((storage.buffer.cursor_up - packet) * sizeof(std::uint32_t));
    Require(bytes == 16, "SET_PREDICATION is not four dwords");
    Require(sceAgcDcbSetZPassPredicationEnableGetSize() == bytes, "z-pass predication size");
    Require(sceAgcDcbSetPredicationDisableGetSize() == bytes, "predication disable size");
    Require(sceAgcDcbSetBoolPredicationEnableGetSize() == bytes, "bool predication size");
}

void VerifyPacket() {
    Storage storage;
    auto* packet = sceAgcDcbSetIndexCount(&storage.buffer, 7);
    const auto header = packet[0];
    Require(sceAgcSetPacketPredication(packet, 1) == 0 && packet[0] == (header | 1u) && packet[1] == 7, "predication bit not set");
    Require(sceAgcSetPacketPredication(packet, 0) == 0 && packet[0] == header, "predication bit not cleared");
    ExpectFailure([&] { sceAgcSetPacketPredication(packet, 2); });
    ExpectFailure([&] { sceAgcSetPacketPredication(nullptr, 1); });
    std::uint32_t filler = 0x80000000u;
    ExpectFailure([&] { sceAgcSetPacketPredication(&filler, 1); });
    Require(packet[0] == header && filler == 0x80000000u, "rejected calls modified a packet");
}

void VerifyRange() {
    Storage storage;
    auto* first = sceAgcDcbSetIndexCount(&storage.buffer, 3);
    *storage.buffer.cursor_up++ = 0x80000000u;
    *storage.buffer.cursor_up++ = 0xffff1000u;
    auto* second = sceAgcDcbSetIndexBuffer(&storage.buffer, 0x1000);
    auto* third = sceAgcDcbSetIndexCount(&storage.buffer, 5);
    const std::array headers{first[0], second[0], third[0]};
    Require(sceAgcSetRangePredication(first, storage.buffer.cursor_up, 1) == 0, "range predication failed");
    Require(first[0] == (headers[0] | 1u) && second[0] == (headers[1] | 1u) && third[0] == (headers[2] | 1u), "range did not predicate every packet");
    Require(first[2] == 0x80000000u && first[3] == 0xffff1000u && second[1] == 0x1000u && third[1] == 5u, "range predication changed a payload, filler or pad");
    Require(sceAgcSetRangePredication(second, third, 0) == 0 && second[0] == headers[1] && third[0] == (headers[2] | 1u), "range end is not exclusive");
    Require(sceAgcSetRangePredication(first, first, 0) == 0 && first[0] == (headers[0] | 1u), "empty range changed a packet");
    ExpectFailure([&] { sceAgcSetRangePredication(first, first + 1, 1); });
    ExpectFailure([&] { sceAgcSetRangePredication(second, first, 1); });
    ExpectFailure([&] { sceAgcSetRangePredication(first, third, 2); });
    std::array<std::uint32_t, 2> invalid{0x40000000u, 0};
    ExpectFailure([&] { sceAgcSetRangePredication(invalid.data(), invalid.data() + invalid.size(), 1); });
}

}

namespace {

const Testing::Case sizes{"PredicationGetSize_EveryEnableKind_MatchesSetPredicationPacket", [] {
    VerifySizes();
}};

const Testing::Case packet{"SetPacketPredication_ValidOrInvalidArguments_TogglesBitOrRejects", [] {
    VerifyPacket();
}};

const Testing::Case range{"SetRangePredication_PacketRange_PredicatesEveryPacketOrRejects", [] {
    VerifyRange();
}};

} // namespace
