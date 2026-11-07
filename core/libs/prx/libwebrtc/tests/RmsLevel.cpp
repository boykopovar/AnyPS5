#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

using ConstructFn = void (APS5_VABI*)(WebRtcRmsLevel*);
using AnalyzeFn = void (APS5_VABI*)(WebRtcRmsLevel*, const std::int16_t*, std::size_t);
using AverageFn = int (APS5_VABI*)(WebRtcRmsLevel*);

const auto construct = reinterpret_cast<ConstructFn>(&_ZN6webrtc8RmsLevelC1Ev_nid_postfix);
const auto destroy = reinterpret_cast<ConstructFn>(&_ZN6webrtc8RmsLevelD1Ev_nid_postfix);
const auto analyze = reinterpret_cast<AnalyzeFn>(&_ZN6webrtc8RmsLevel7AnalyzeEN3rtc9ArrayViewIKsLln4711EEE_nid_postfix);
const auto average = reinterpret_cast<AverageFn>(&_ZN6webrtc8RmsLevel7AverageEv_nid_postfix);

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc RmsLevel check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

bool Cleared(const WebRtcRmsLevel& level) {
    return level.sumSquare == 0.f && level.sampleCount == 0 && level.maxSumSquare == 0.f && !level.hasBlockSize;
}

WebRtcRmsLevel Constructed() {
    WebRtcRmsLevel level;
    std::memset(&level, 0x5a, sizeof(level));
    construct(&level);
    return level;
}

void Analyze(WebRtcRmsLevel& level, const std::vector<std::int16_t>& samples) {
    analyze(&level, samples.data(), samples.size());
}

void TestConstruct() {
    auto level = Constructed();
    Require(Cleared(level));
    Require(average(&level) == 127);
    Require(Cleared(level));
    destroy(&level);
}

void TestLevels() {
    auto level = Constructed();
    Analyze(level, std::vector<std::int16_t>(160, 1000));
    Require(level.sampleCount == 160 && level.hasBlockSize && level.blockSize == 160);
    Require(level.sumSquare == 1.6e8f && level.maxSumSquare == 1.6e8f);
    Analyze(level, std::vector<std::int16_t>(160, 0));
    Require(level.sampleCount == 320 && level.sumSquare == 1.6e8f && level.maxSumSquare == 1.6e8f);
    Require(average(&level) == 33);
    Require(Cleared(level));

    Analyze(level, std::vector<std::int16_t>(160, 1000));
    Require(average(&level) == 30);
    Analyze(level, std::vector<std::int16_t>(160, 1));
    Require(average(&level) == 90);
    std::vector<std::int16_t> fullScale(160);
    for (std::size_t index = 0; index < fullScale.size(); ++index) fullScale[index] = index % 2 == 0 ? 32767 : -32767;
    Analyze(level, fullScale);
    Require(average(&level) == 0);
    Analyze(level, std::vector<std::int16_t>(160, 0));
    Require(average(&level) == 127);
    std::vector<std::int16_t> faint(4800, 0);
    faint[17] = 1;
    Analyze(level, faint);
    Require(average(&level) == 126);
}

void TestBlockSize() {
    auto level = Constructed();
    Analyze(level, std::vector<std::int16_t>(160, 1000));
    analyze(&level, nullptr, 0);
    Require(level.sampleCount == 160 && level.blockSize == 160);
    Analyze(level, std::vector<std::int16_t>(80, 0));
    Require(level.sampleCount == 80 && level.blockSize == 80 && level.sumSquare == 0.f && level.maxSumSquare == 0.f);
    Require(average(&level) == 127);
}

void TestNull() {
    RequireInvalidArgument([] { construct(nullptr); });
    RequireInvalidArgument([] { destroy(nullptr); });
    RequireInvalidArgument([] { analyze(nullptr, nullptr, 0); });
    RequireInvalidArgument([] { average(nullptr); });
}

}

int main() {
    TestConstruct();
    TestLevels();
    TestBlockSize();
    TestNull();
    std::puts("libwebrtc RmsLevel tests passed");
    return 0;
}
