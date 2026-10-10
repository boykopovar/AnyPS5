#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

extern "C" {
std::uint32_t* APS5_VABI sceAgcAcbWaitUntilSafeForRendering(CommandBuffer*, std::uint32_t, std::uint32_t);
std::uint32_t* APS5_VABI sceAgcDcbWaitUntilSafeForRendering(CommandBuffer*, std::uint32_t, std::uint32_t);
std::uint32_t APS5_VABI sceAgcDriverGetWaitRenderingPacketSizeInDwords();
std::uint32_t APS5_VABI sceAgcDriverWaitUntilSafeForRendering(std::uint32_t**, std::uint32_t, std::uint32_t, std::uint32_t, int);
}

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

constexpr std::size_t Guard = 8;

template <typename TEmit>
std::array<std::uint32_t, AgcDriver::RenderingWaitPacketWords + Guard> Emit(TEmit emit, std::uint32_t handle, std::uint32_t index) {
    std::array<std::uint32_t, AgcDriver::RenderingWaitPacketWords + Guard> words{};
    std::fill(words.begin(), words.end(), 0xccccccccu);
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
    const auto* packet = emit(&buffer, handle, index);
    check(packet == words.data(), "the rendering wait is not written at the cursor");
    check((buffer.cursor_up - packet) * sizeof(std::uint32_t) == AgcDriver::RenderingWaitPacketWords * sizeof(std::uint32_t),
        "the rendering wait does not advance the cursor by its own size");
    check(packet[0] == AgcDriver::RenderingWaitPacketHeader, "the rendering wait header is wrong");
    check(packet[1] == handle && packet[2] == index && packet[3] == 0, "the rendering wait arguments are wrong");
    for (std::size_t word = AgcDriver::RenderingWaitPacketWords; word < words.size(); ++word) {
        check(words[word] == 0xccccccccu, "the rendering wait writes past its packet");
    }
    return words;
}

}

int main() {
    try {
        check(sceAgcDriverGetWaitRenderingPacketSizeInDwords() == AgcDriver::RenderingWaitPacketWords,
            "the driver reports a different rendering wait size");

        const auto compute = Emit(sceAgcAcbWaitUntilSafeForRendering, 7, 3);
        const auto draw = Emit(sceAgcDcbWaitUntilSafeForRendering, 7, 3);
        check(compute == draw, "the compute and draw rendering waits differ");

        const auto other = Emit(sceAgcAcbWaitUntilSafeForRendering, 2, 1);
        check(other[1] == 2 && other[2] == 1, "the rendering wait ignores its arguments");
        check(other[0] == compute[0], "the rendering wait header depends on its arguments");

        std::array<std::uint32_t, AgcDriver::RenderingWaitPacketWords> driverWords{};
        std::uint32_t* cursor = driverWords.data();
        check(sceAgcDriverWaitUntilSafeForRendering(&cursor, driverWords.size(), 0, 7, 3) == 0, "the driver rejected the packet");
        for (std::size_t word = 0; word < driverWords.size(); ++word) {
            check(driverWords[word] == compute[word], "the library and the driver disagree on a packet word");
        }

        bool rejected = false;
        try {
            std::array<std::uint32_t, 2> tooSmall{};
            CommandBuffer buffer{tooSmall.data(), tooSmall.data() + tooSmall.size(), tooSmall.data(), tooSmall.data() + tooSmall.size(), nullptr, nullptr, 0};
            sceAgcAcbWaitUntilSafeForRendering(&buffer, 7, 3);
        } catch (const std::exception&) {
            rejected = true;
        }
        check(rejected, "a rendering wait that does not fit is accepted");

        LibcRunShutdown_nid_postfix();
        std::puts("AGC rendering wait tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}