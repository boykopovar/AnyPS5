#include "prx/libSceAgc/DcbFlow/include/Control.hpp"

#include "prx/libSceAgc/Command/include/Control.hpp"
#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

std::vector<std::uint32_t> resetQueueGroup(std::uint32_t bit, std::uint32_t state) {
    const std::uint32_t stateThree = state == 3u ? 0x00840f80u : 0x00000000u;
    switch (bit) {
        case 0x001u:
            return {0xc0039f00u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u};
        case 0x002u:
            return {0xc0036300u, 0x00000000u, 0x00000000u, 0x80000000u, 0x00000000u};
        case 0x004u:
            return {0xc0036400u, 0x00000000u, stateThree, 0x80000000u, 0x00000000u};
        case 0x008u:
            return {0xc0002f00u, 0x00000001u};
        case 0x010u:
            return {0xc0017a00u, 0x20000243u, 0x00000480u, 0xc0012600u, 0x00000000u, 0x00000000u, 0xc0001300u, 0xffffffffu};
        case 0x020u:
            return {0xc0021102u, 0x00000001u, 0x00000000u, 0x00000000u, 0xc0021100u, 0x00000001u, 0x00000000u, 0x00000000u};
        case 0x040u:
            return {0xc0065800u, 0x86007fc0u, 0xfffffffeu, 0x000000ffu, 0x00000001u, 0x00000000u, 0x00000019u, 0x0000c3e1u, 0xc0004600u, 0x0000002eu, 0xc0004600u, 0x0000002cu};
        case 0x080u:
            return {0xc0004600u, 0x00000407u};
        case 0x100u:
            return {0xc0004600u, 0x00000410u};
        case 0x200u:
            return {0xc0024600u, 0x00000138u, 0x07fffc00u, 0x00000000u};
        case 0x800u:
            return {0xc0036400u, 0x00000000u, stateThree, 0x80000000u, 0x00000000u};
        default:
            throw std::runtime_error(std::string("sceAgcDcbResetQueue: unsupported op bit ") + std::to_string(bit));
    }
}

}


extern "C" {

// unknown signature
APS5_EXPORT("zARR5aCmkoY", sceAgcDcbA_zARR5aCmkoY);
void* APS5_VABI sceAgcDcbA_zARR5aCmkoY(void) {
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}


uint32_t* APS5_VABI sceAgcDcbJump(CommandBuffer* buf, uint8_t mode, uint8_t cache_policy, const uint32_t* target, uint32_t size_in_dwords) {
    return Agc::Command::WriteJump(buf, mode, cache_policy, target, size_in_dwords, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbJumpGetSize() {
    return 16;
}

std::uint32_t* APS5_VABI sceAgcDcbResetQueue(CommandBuffer* buf, std::uint32_t op, std::uint32_t state) {
    Agc::Command::CheckBits(op, 0xfffu, __func__);
    Agc::Command::CheckBits(state, 0xfu, __func__);

    Agc::Command::Require((op & 0x400u) == 0u, __func__, "unsupported op bit 0x400");

    const std::uint32_t envelopeFlag = state == 2u ? 0x00000008u : 0x00000000u;
    std::vector<std::uint32_t> words{
        0xffff1000u,
        0xc0027904u, 0x00000342u, 0xce200000u | (op & 0xffffu), envelopeFlag};

    constexpr std::array<std::uint32_t, 11> order{0x001u, 0x002u, 0x004u, 0x008u, 0x010u, 0x020u, 0x040u, 0x080u, 0x100u, 0x200u, 0x800u};
    for (const auto bit : order) {
        if ((op & bit) != 0u) {
            const auto group = resetQueueGroup(bit, state);
            words.insert(words.end(), group.begin(), group.end());
        }
    }

    words.insert(words.end(), {0xc0017904u, 0x00000342u, 0xcea00000u});

    auto* packet = Agc::Command::Allocate(buf, static_cast<std::uint32_t>(words.size()), __func__);
    std::copy(words.begin(), words.end(), packet);
    return packet;
}

std::uint32_t* APS5_VABI sceAgcDcbClearState(CommandBuffer* buf, std::uint32_t command) {
    Agc::Command::CheckBits(command, 0xfu, __func__);
    return Agc::Command::Emit(buf, 0x12u, {command}, __func__);
}

uint32_t* APS5_VABI sceAgcDcbRewind(CommandBuffer* buf, uint32_t initial_state) {
    return Agc::Command::WriteRewind(buf, initial_state, __func__);
}

uint32_t APS5_VABI sceAgcDcbRewindGetSize(void) {
    return 8;
}

uint32_t* APS5_VABI sceAgcDcbWaitUntilSafeForRendering(CommandBuffer* buf, uint32_t video_out_handle, uint32_t display_buffer_index) {
    auto* packet = Agc::Command::Allocate(buf, AgcDriver::RenderingWaitPacketWords, __func__);
    packet[0] = AgcDriver::RenderingWaitPacketHeader;
    packet[1] = video_out_handle;
    packet[2] = display_buffer_index;
    packet[3] = 0;
    return packet;
}

}
