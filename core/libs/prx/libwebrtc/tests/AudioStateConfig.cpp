#include "prx/libwebrtc/libwebrtc.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void TestConstructorClearsEveryReference() {
    WebRtcAudioStateConfig config;
    std::memset(&config, 0xA5, sizeof(config));
    _ZN6webrtc10AudioState6ConfigC1Ev_nid_postfix(&config);
    Require(config.audioMixer == nullptr);
    Require(config.audioProcessing == nullptr);
    Require(config.audioDeviceModule == nullptr);
    Require(config.asyncAudioProcessingFactory == nullptr);
}

void TestNullConfigThrows() {
    try {
        _ZN6webrtc10AudioState6ConfigC1Ev_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

}

int main() {
    TestConstructorClearsEveryReference();
    TestNullConfigThrows();
    return 0;
}
