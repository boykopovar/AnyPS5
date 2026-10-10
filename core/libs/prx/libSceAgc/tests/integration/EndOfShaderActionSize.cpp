#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstdint>

extern "C" {
std::uint32_t APS5_VABI sceAgcDcbQueueEndOfShaderActionGetSize();
std::uint32_t APS5_VABI sceAgcAcbQueueEndOfShaderActionGetSize();
std::uint32_t* APS5_VABI sceAgcCbReleaseMem(CommandBuffer*, std::uint8_t, std::uint16_t, std::uint8_t, std::uint8_t, const volatile Label*, std::uint8_t, std::uint64_t, std::uint16_t, std::uint16_t, std::uint8_t, std::uint32_t);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

std::uint32_t ReleaseMemBytes() {
    std::array<std::uint32_t, 16> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
    const auto* packet = sceAgcCbReleaseMem(&buffer, 0x2f, 0, 0, 0, nullptr, 0, 0, 0, 0, 0, 0);
    RequireEqual((packet[0] >> 8u) & 0xffu, 0x49u, "end-of-shader action is a RELEASE_MEM packet");
    return static_cast<std::uint32_t>((buffer.cursor_up - packet) * sizeof(std::uint32_t));
}

const Case releaseMem{"CbReleaseMem_EndOfShaderAction_Writes32Bytes", [] {
    RequireEqual(ReleaseMemBytes(), 32u, "end-of-shader RELEASE_MEM size");
}};

const Case dcbSize{"DcbQueueEndOfShaderActionGetSize_Default_MatchesReleaseMem", [] {
    RequireEqual(sceAgcDcbQueueEndOfShaderActionGetSize(), ReleaseMemBytes(), "DCB end-of-shader action size");
}};

const Case acbSize{"AcbQueueEndOfShaderActionGetSize_Default_MatchesDcb", [] {
    RequireEqual(sceAgcAcbQueueEndOfShaderActionGetSize(), sceAgcDcbQueueEndOfShaderActionGetSize(), "ACB end-of-shader action size");
}};

} // namespace

int main(int argc, char** argv) {
    const int result = Testing::Run(argc, argv);
    LibcRunShutdown_nid_postfix();
    return result;
}
