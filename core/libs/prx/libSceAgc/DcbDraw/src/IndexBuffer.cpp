#include "prx/libSceAgc/DcbDraw/include/IndexBuffer.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcDcbSetIndexBuffer(CommandBuffer* buf, std::uint64_t indexAddress) {
    Agc::Command::CheckAddress(indexAddress, 2, __func__);
    return Agc::Command::Emit(buf, 0x26u, {static_cast<std::uint32_t>(indexAddress), static_cast<std::uint32_t>(indexAddress >> 32u)}, __func__);
}

uint32_t* APS5_VABI sceAgcDcbSetIndexCount(CommandBuffer* buf, uint32_t index_count) {
 (void)buf;
 (void)index_count;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

std::uint32_t* APS5_VABI sceAgcDcbSetIndexSize(CommandBuffer* buf, std::uint8_t indexSize, std::uint8_t cachePolicy) {
    Agc::Command::Require(indexSize <= 2, __func__, "invalid index element size");
    Agc::Command::CheckBits(cachePolicy, 3, __func__);
    return Agc::Command::Emit(buf, 0x7au, {0x20000243u, 0x400u | indexSize | (static_cast<std::uint32_t>(cachePolicy) << 6u)}, __func__);
}

}
