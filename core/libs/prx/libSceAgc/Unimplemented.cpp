#include "prx/libSceAgc/include/Agc.hpp"

#include "prx/libSceAgc/Cb/include/Dispatch.hpp"
#include "prx/libSceAgc/Cb/include/Registers.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <array>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

alignas(256) constexpr std::array<std::uint32_t, 69> kPatternFillCode{
    0xbfa00003u, 0xd7460002u, 0x04010c0au, 0x7da80408u, 0xbf88003fu, 0x7e000c09u, 0xbf070980u, 0x858a807eu,
    0x7e005700u, 0x100000ffu, 0x4f800000u, 0x7e060f00u, 0xd5766a00u, 0x02020609u, 0x7d8a0280u, 0x4c020080u,
    0x02000101u, 0xd56a0001u, 0x00020700u, 0x4c000303u, 0x4a020303u, 0x02000101u, 0xd56a0000u, 0x00020500u,
    0xd5690001u, 0x00020009u, 0x4c060302u, 0x7d860609u, 0x7d8c02f9u, 0x06068c02u, 0x87ea6a0cu, 0x50000080u,
    0xd5286a00u, 0x003200c1u, 0xd5010000u, 0x002a00c1u, 0xd5690000u, 0x00020009u, 0x4c000102u, 0x7d0a0080u,
    0xbe88246au, 0xbf880015u, 0x7d0a0081u, 0xbe8a246au, 0xbf88000cu, 0x7d0a0082u, 0xbeea246au, 0xbf880003u,
    0x7e000207u, 0xe0102000u, 0x80000002u, 0x8afe7e6au, 0xbf880003u, 0x7e000206u, 0xe0102000u, 0x80000002u,
    0xbefe046au, 0x8afe7e0au, 0xbf880003u, 0x7e000205u, 0xe0102000u, 0x80000002u, 0xbefe040au, 0x8afe7e08u,
    0xbf880003u, 0x7e000204u, 0xe0102000u, 0x80000002u, 0xbf810000u};

}

extern "C" {

std::uint32_t* APS5_VABI sceAgcCbMemsetExclusive(CommandBuffer* buffer, void* destination, const void* pattern, std::uint64_t size) {
    Agc::Command::Require(pattern != nullptr, __func__, "null pattern");
    std::array<std::uint32_t, 4> patternWords{};
    std::memcpy(patternWords.data(), pattern, sizeof(patternWords));
    const auto blockCount = static_cast<std::uint32_t>(size >> 4u);
    const auto dwordCount = blockCount * 4u;
    const auto groupCount = static_cast<std::uint32_t>((static_cast<std::uint64_t>(dwordCount) + 63u) / 64u);
    const auto destinationAddress = reinterpret_cast<std::uintptr_t>(destination);
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(kPatternFillCode.data());
    const std::array<ShaderRegister, 3> shape{{{0x207u, 64u}, {0x208u, 1u}, {0x209u, 1u}}};
    const std::array<ShaderRegister, 2> program{{{0x20cu, static_cast<std::uint32_t>(codeAddress >> 8u)}, {0x20du, static_cast<std::uint32_t>(codeAddress >> 40u)}}};
    const std::array<ShaderRegister, 1> resource{{{0x213u, (10u << 1u) | (1u << 7u)}}};
    const std::array<ShaderRegister, 10> userData{{
        {0x240u, static_cast<std::uint32_t>(destinationAddress)},
        {0x241u, static_cast<std::uint32_t>(destinationAddress >> 32u) | (4u << 16u)},
        {0x242u, dwordCount},
        {0x243u, 0x00014004u},
        {0x244u, patternWords[0]},
        {0x245u, patternWords[1]},
        {0x246u, patternWords[2]},
        {0x247u, patternWords[3]},
        {0x248u, dwordCount},
        {0x249u, 4u},
    }};
    auto* first = sceAgcCbSetShRegistersDirect(buffer, shape.data(), static_cast<std::uint32_t>(shape.size()));
    sceAgcCbSetShRegistersDirect(buffer, program.data(), static_cast<std::uint32_t>(program.size()));
    sceAgcCbSetShRegistersDirect(buffer, resource.data(), static_cast<std::uint32_t>(resource.size()));
    sceAgcCbSetShRegistersDirect(buffer, userData.data(), static_cast<std::uint32_t>(userData.size()));
    sceAgcCbDispatch(buffer, groupCount, 1u, 1u, 0u);
    return first;
}

int APS5_VABI sceAgcAcbInitializeDefaultHardwareState() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
