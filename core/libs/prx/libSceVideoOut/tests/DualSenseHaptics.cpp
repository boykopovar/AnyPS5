#include "prx/libSceVideoOut/include/DualSenseHaptics.hpp"
#include "prx/libScePad/include/PadHapticBuffer.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "dualsense_haptics: %s\n", message);
    std::exit(1);
}

void ReportLayout() {
    std::array<std::int8_t, DualSenseHaptics::SamplesBytes> samples{};
    for (std::size_t index = 0; index < samples.size(); ++index) samples[index] = static_cast<std::int8_t>(index * 3 - 96);
    const auto report = DualSenseHaptics::Report(5, 0x2A, samples);
    std::array<std::uint8_t, DualSenseHaptics::ReportBytes> expected{};
    expected[0] = 0x32;
    expected[1] = 0x50;
    expected[2] = 0x91;
    expected[3] = 0x07;
    expected[4] = 0xFE;
    for (std::size_t index = 5; index <= 9; ++index) expected[index] = DualSenseHaptics::ControllerQueueFrames;
    expected[10] = 0x2A;
    expected[11] = 0x92;
    expected[12] = 64;
    for (std::size_t index = 0; index < samples.size(); ++index) expected[13 + index] = static_cast<std::uint8_t>(samples[index]);
    expected[138] = 0xFB;
    expected[139] = 0x6F;
    expected[140] = 0x69;
    expected[141] = 0xCC;
    for (std::size_t index = 0; index < expected.size(); ++index) {
        if (report[index] == expected[index]) continue;
        std::fprintf(stderr, "dualsense_haptics: report byte %zu is 0x%02x, expected 0x%02x\n", index, report[index], expected[index]);
        std::exit(1);
    }
}

void PowerReportLayout() {
    const auto report = DualSenseHaptics::PowerReport(3);
    std::array<std::uint8_t, DualSenseHaptics::PowerReportBytes> expected{};
    expected[0] = 0x31;
    expected[1] = 0x30;
    expected[2] = 0x10;
    expected[4] = 0x62;
    expected[42] = 0x01;
    expected[74] = 0x82;
    expected[75] = 0x1A;
    expected[76] = 0x1F;
    expected[77] = 0x26;
    Require(report == expected, "the power report has the wrong layout or CRC32");
}

void BufferAveragesTo3kHz() {
    PadHapticBuffer buffer;
    std::vector<float> pad(48 * 4 * 2, 0.0f);
    for (std::size_t frame = 0; frame < 48 * 2; ++frame) {
        pad[frame * 4 + 2] = frame < 16 ? 1.0f : frame < 32 ? -0.5f : 0.25f;
        pad[frame * 4 + 3] = -1.0f;
    }
    buffer.Push(pad.data() + 2, pad.data() + 3, 4, 48 * 2, 48000);
    Require(buffer.Frames() == 6, "96 frames at 48 kHz are 6 frames at 3 kHz");
    for (int round = 0; round < 26; ++round) buffer.Push(pad.data() + 2, pad.data() + 3, 4, 48 * 2, 48000);
    Require(buffer.Frames() == PadHapticBuffer::MaxFrames, "the buffer keeps only its newest frames");
    std::array<std::int8_t, PadHapticBuffer::BlockFrames * 2> block{};
    Require(buffer.Take(block), "a full block is taken");
    Require(buffer.Frames() == PadHapticBuffer::MaxFrames - PadHapticBuffer::BlockFrames, "taking a block removes its frames");
    const std::array<std::int8_t, 6> left{127, -64, 32, 32, 32, 32};
    for (std::size_t frame = 0; frame < 6; ++frame) {
        Require(block[frame * 2] == left[(frame + 2) % 6], "channel 0 holds the 16-frame averages in order");
        Require(block[frame * 2 + 1] == -127, "channel 1 holds its own averages");
    }
    PadHapticBuffer partial;
    partial.Push(pad.data() + 2, pad.data() + 3, 4, 20, 48000);
    Require(!partial.Take(block), "less than a block is not taken");
}

void BufferFollowsTheInputRate() {
    PadHapticBuffer buffer;
    std::vector<float> pad(441 * 4, 0.5f);
    buffer.Push(pad.data(), pad.data() + 1, 4, 441, 44100);
    Require(buffer.Frames() == 30, "441 frames at 44.1 kHz are 30 frames at 3 kHz");
}

}

int main() {
    ReportLayout();
    PowerReportLayout();
    BufferAveragesTo3kHz();
    BufferFollowsTheInputRate();
    std::puts("dualsense haptics tests passed");
    return 0;
}
