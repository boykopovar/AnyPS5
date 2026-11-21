#include "prx/libSceAgc/DcbDraw/include/Stats.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcDcbGetLodStats(CommandBuffer* buf, std::uint8_t cachePolicy, const volatile void* buffer, std::uint32_t bufferSizeInBytes, std::uint32_t resetCount, std::uint8_t forceReset, std::uint8_t reportAndReset, std::uint32_t reportingIntervalIn100kClocks) {
    Agc::Command::CheckBits(reportAndReset, 1, __func__);
    Agc::Command::CheckBits(reportingIntervalIn100kClocks, 0xffu, __func__);
    const auto address = reinterpret_cast<std::uintptr_t>(buffer);
    const auto control = (static_cast<std::uint32_t>(cachePolicy & 3u) << 28u) | (static_cast<std::uint32_t>(reportAndReset) << 19u) | (static_cast<std::uint32_t>(forceReset & 1u) << 18u) | ((resetCount & 0xffu) << 10u) | (reportingIntervalIn100kClocks << 2u);
    return Agc::Command::Emit(buf, 0x8eu, {bufferSizeInBytes, static_cast<std::uint32_t>(address & ~std::uintptr_t{0x3f}), static_cast<std::uint32_t>(address >> 32u), control}, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbBeginOcclusionQueryGetSize(std::int32_t queryType) {
    if (queryType == 1) return 0x120u;
    return queryType == 0 ? 0x10u : 0u;
}

std::uint32_t APS5_VABI sceAgcDcbEndOcclusionQueryGetSize() {
    return 0x10u;
}

}
