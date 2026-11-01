#include "prx/libwebrtc/libwebrtc.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc AudioCodecInfo check failed at line %u\n", location.line());
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

void Check(int sampleRateHz, std::uint64_t numChannels, int defaultBitrateBps, int minBitrateBps, int maxBitrateBps) {
    struct Guarded {
        std::array<unsigned char, 16> before;
        WebRtcAudioCodecInfo info;
        std::array<unsigned char, 16> after;
    } guarded;
    std::memset(&guarded, 0xA5, sizeof(guarded));
    guarded.info.allowComfortNoise = false;
    guarded.info.supportsNetworkAdaption = true;
    _ZN6webrtc14AudioCodecInfoC1Eimiii_nid_postfix(&guarded.info, sampleRateHz, numChannels, defaultBitrateBps, minBitrateBps, maxBitrateBps);
    Require(guarded.info.sampleRateHz == sampleRateHz);
    Require(guarded.info.numChannels == numChannels);
    Require(guarded.info.defaultBitrateBps == defaultBitrateBps);
    Require(guarded.info.minBitrateBps == minBitrateBps);
    Require(guarded.info.maxBitrateBps == maxBitrateBps);
    Require(guarded.info.allowComfortNoise);
    Require(!guarded.info.supportsNetworkAdaption);
    for (auto byte : guarded.before) Require(byte == 0xA5);
    for (auto byte : guarded.after) Require(byte == 0xA5);
}

void TestLocalDefensiveChecks() {
    WebRtcAudioCodecInfo info {};
    auto construct = [&](int rate, std::uint64_t channels, int bitrate, int minimum, int maximum) {
        _ZN6webrtc14AudioCodecInfoC1Eimiii_nid_postfix(&info, rate, channels, bitrate, minimum, maximum);
    };
    RequireInvalidArgument([&] { construct(0, 1, 32000, 0, 64000); });
    RequireInvalidArgument([&] { construct(-1, 1, 32000, 0, 64000); });
    RequireInvalidArgument([&] { construct(48000, 0, 32000, 0, 64000); });
    RequireInvalidArgument([&] { construct(48000, 1, 32000, -1, 64000); });
    RequireInvalidArgument([&] { construct(48000, 1, 32000, 32001, 64000); });
    RequireInvalidArgument([&] { construct(48000, 1, 32000, 0, 31999); });
    RequireInvalidArgument([] { _ZN6webrtc14AudioCodecInfoC1Eimiii_nid_postfix(nullptr, 48000, 1, 32000, 0, 64000); });
}

}

int main() {
    try {
        Check(48000, 1, 32000, 6000, 64000);
        Check(16000, 2, 24000, 24000, 24000);
        Check(96000, 6, 128000, 0, 256000);
        Check(1, std::numeric_limits<std::uint64_t>::max(), 0, 0, 0);
        Check(std::numeric_limits<int>::max(), 1, std::numeric_limits<int>::max(), 0, std::numeric_limits<int>::max());
        TestLocalDefensiveChecks();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    std::puts("libwebrtc AudioCodecInfo tests passed");
}
