#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc AudioSourceInterface::options check failed at line %u\n", location.line());
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

void TestEveryOptionIsUnset() {
    WebRtcAudioOptions options;
    std::memset(&options, 0xA5, sizeof(options));
    int source = 0;
    Require(_ZNK6webrtc20AudioSourceInterface7optionsEv_nid_postfix(&options, &source) == &options);
    Require(!options.echoCancellation.engaged && !options.autoGainControl.engaged);
    Require(!options.noiseSuppression.engaged && !options.highpassFilter.engaged && !options.stereoSwapping.engaged);
    Require(!options.audioJitterBufferMaxPackets.engaged && !options.audioJitterBufferFastAccelerate.engaged);
    Require(!options.audioJitterBufferMinDelayMs.engaged && !options.audioNetworkAdaptor.engaged);
    Require(!options.audioNetworkAdaptorConfig.engaged && !options.initRecordingOnSend.engaged);
}

void TestSourceIsNotRead() {
    WebRtcAudioOptions options;
    std::memset(&options, 0xA5, sizeof(options));
    Require(_ZNK6webrtc20AudioSourceInterface7optionsEv_nid_postfix(&options, nullptr) == &options);
    Require(!options.echoCancellation.engaged && !options.initRecordingOnSend.engaged);
}

void TestNullResultThrows() {
    int source = 0;
    RequireInvalidArgument([&] { _ZNK6webrtc20AudioSourceInterface7optionsEv_nid_postfix(nullptr, &source); });
}

}

int main() {
    TestEveryOptionIsUnset();
    TestSourceIsNotRead();
    TestNullResultThrows();
    return 0;
}
