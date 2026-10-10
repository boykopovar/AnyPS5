#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <Testing/Test.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

extern "C" {
std::uint32_t* APS5_VABI sceAgcDcbWriteData(CommandBuffer*, std::uint8_t, std::uint8_t, std::uint64_t, const void*, std::uint32_t, std::uint8_t, std::uint8_t);
std::uint32_t* APS5_VABI sceAgcAcbWriteData(CommandBuffer*, std::uint8_t, std::uint8_t, std::uint64_t, const void*, std::uint32_t, std::uint8_t, std::uint8_t);
int APS5_VABI sceAgcWriteDataPatchSetAddressOrOffset(std::uint32_t*, std::uint64_t);
int APS5_VABI sceAgcAsyncWriteDataPatchSetAddressOrOffset(std::uint32_t*, std::uint64_t);
int APS5_VABI sceAgcWriteDataPatchSetDst(std::uint32_t*, std::uint8_t);
int APS5_VABI sceAgcAsyncWriteDataPatchSetDst(std::uint32_t*, std::uint8_t);
int APS5_VABI sceAgcWriteDataPatchSetCachePolicy(std::uint32_t*, std::uint8_t);
int APS5_VABI sceAgcAsyncWriteDataPatchSetCachePolicy(std::uint32_t*, std::uint8_t);
}

namespace {

constexpr std::array<std::uint32_t, 3> Payload{0x11111111u, 0x22222222u, 0x33333333u};
constexpr std::uint64_t Address = 0x0000123456789ab0ull;
constexpr std::uint64_t OtherAddress = 0x0000fedcba987650ull;

using Testing::Require;

template<typename TAction>
void ExpectFailure(TAction action) {
    Testing::RequireThrows<std::runtime_error>(action, "expected invalid input to fail");
}

struct Packet {
    std::array<std::uint32_t, 16> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
};

struct Fields {
    std::uint8_t dst;
    std::uint8_t cachePolicy;
    std::uint64_t address;
    std::uint8_t writeConfirm;
};

using Writer = std::uint32_t* (APS5_VABI *)(CommandBuffer*, std::uint8_t, std::uint8_t, std::uint64_t, const void*, std::uint32_t, std::uint8_t, std::uint8_t);
using AddressPatch = int (APS5_VABI *)(std::uint32_t*, std::uint64_t);
using FieldPatch = int (APS5_VABI *)(std::uint32_t*, std::uint8_t);

struct Variant {
    Writer writer;
    AddressPatch setAddress;
    FieldPatch setDst;
    FieldPatch setCachePolicy;
    std::uint8_t maxDst;
};

std::uint32_t* build(Packet& packet, const Variant& variant, const Fields& fields) {
    return variant.writer(&packet.buffer, fields.dst, fields.cachePolicy, fields.address, Payload.data(), static_cast<std::uint32_t>(Payload.size()), 1, fields.writeConfirm);
}

void expectPatched(const Variant& variant, const Fields& from, const Fields& to, const char* message) {
    Packet patched;
    auto* packet = build(patched, variant, from);
    if (from.address != to.address) Require(variant.setAddress(packet, to.address) == 0, "address patch failed");
    if (from.dst != to.dst) Require(variant.setDst(packet, to.dst) == 0, "destination patch failed");
    if (from.cachePolicy != to.cachePolicy) Require(variant.setCachePolicy(packet, to.cachePolicy) == 0, "cache policy patch failed");
    Packet expected;
    build(expected, variant, to);
    Require(patched.words == expected.words, message);
}

void VerifyVariant(const Variant& variant) {
    for (std::uint8_t from = 1; from <= variant.maxDst; ++from) {
        for (std::uint8_t to = 1; to <= variant.maxDst; ++to) {
            expectPatched(variant, {from, 1, Address, 1}, {to, 1, Address, 1}, "destination patch does not match a packet built with that destination");
        }
    }
    expectPatched(variant, {5, 1, Address, 0}, {0, 1, Address, 0}, "register destination patch does not match a packet built with it");
    for (std::uint8_t from = 0; from < 4; ++from) {
        for (std::uint8_t to = 0; to < 4; ++to) {
            expectPatched(variant, {5, from, Address, 1}, {5, to, Address, 1}, "cache policy patch does not match a packet built with that policy");
        }
    }
    expectPatched(variant, {5, 2, Address, 1}, {5, 2, OtherAddress, 1}, "address patch does not match a packet built with that address");
    expectPatched(variant, {2, 0, Address, 1}, {variant.maxDst, 3, OtherAddress, 1}, "combined patches do not match a packet built with those fields");

    Packet packet;
    auto* written = build(packet, variant, {5, 1, Address, 1});
    const auto before = packet.words;
    ExpectFailure([&] { variant.setDst(written, static_cast<std::uint8_t>(variant.maxDst + 1u)); });
    ExpectFailure([&] { variant.setDst(written, 0); });
    ExpectFailure([&] { variant.setCachePolicy(written, 4); });
    Require(packet.words == before, "rejected patch modified the packet");

    std::array<std::uint32_t, 8> nop{0xc0021000u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    const auto nopBefore = nop;
    ExpectFailure([&] { variant.setAddress(nop.data(), Address); });
    ExpectFailure([&] { variant.setDst(nop.data(), 5); });
    ExpectFailure([&] { variant.setCachePolicy(nop.data(), 1); });
    Require(nop == nopBefore, "patch of another packet modified it");
}

}

namespace {

const Testing::Case draw{"DcbWriteDataPatches_ValidOrInvalidInput_PatchOrReject", [] {
    VerifyVariant({sceAgcDcbWriteData, sceAgcWriteDataPatchSetAddressOrOffset, sceAgcWriteDataPatchSetDst, sceAgcWriteDataPatchSetCachePolicy, 0x1f});
}};

const Testing::Case compute{"AcbWriteDataPatches_ValidOrInvalidInput_PatchOrReject", [] {
    VerifyVariant({sceAgcAcbWriteData, sceAgcAsyncWriteDataPatchSetAddressOrOffset, sceAgcAsyncWriteDataPatchSetDst, sceAgcAsyncWriteDataPatchSetCachePolicy, 0xf});
}};

} // namespace
