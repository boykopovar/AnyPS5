#include "prx/libSceAgc/DcbDraw/include/Stats.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcDcbGetLodStats(CommandBuffer* buf, std::uint8_t cachePolicy, const volatile void* buffer, std::uint32_t bufferSizeInBytes, std::uint32_t resetCount, std::uint8_t forceReset, std::uint8_t reportAndReset, std::uint32_t reportingIntervalIn100kClocks) {
    Agc::Command::CheckBits(cachePolicy, 3, __func__);
    Agc::Command::CheckBits(resetCount, 0xffu, __func__);
    Agc::Command::CheckBits(forceReset, 1, __func__);
    Agc::Command::CheckBits(reportAndReset, 1, __func__);
    Agc::Command::CheckBits(reportingIntervalIn100kClocks, 0xffu, __func__);
    const auto address = reinterpret_cast<std::uintptr_t>(buffer);
    if (address != 0) Agc::Command::CheckAddress(address, 64, __func__);
    const auto control = (static_cast<std::uint32_t>(cachePolicy) << 28u) | (static_cast<std::uint32_t>(reportAndReset) << 19u) | (static_cast<std::uint32_t>(forceReset) << 18u) | (resetCount << 10u) | (reportingIntervalIn100kClocks << 2u);
    // LOD statistics are not collected; a same-sized NOP keeps the packet layout without producing data.
    return Agc::Command::Emit(buf, 0x10u, {bufferSizeInBytes, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), control}, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbBeginOcclusionQueryGetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint32_t APS5_VABI sceAgcDcbEndOcclusionQueryGetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
