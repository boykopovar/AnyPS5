#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc AudioEncoder check failed at line %u\n", location.line());
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

void Fill(WebRtcAnaStats& stats) {
    std::memset(&stats, 0x5A, sizeof(stats));
}

bool Cleared(const WebRtcOptionalUint32& optional) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(&optional);
    return !optional.engaged && bytes[0] == 0 && bytes[1] == 0x5A && bytes[2] == 0x5A && bytes[3] == 0x5A && bytes[5] == 0x5A && bytes[6] == 0x5A && bytes[7] == 0x5A;
}

bool AllCleared(const WebRtcAnaStats& stats) {
    WebRtcOptionalUint32 loss;
    std::memcpy(&loss, &stats.uplinkPacketLossFraction, sizeof(loss));
    return Cleared(stats.bitrateActionCounter) && Cleared(stats.channelActionCounter) && Cleared(stats.dtxActionCounter) && Cleared(stats.fecActionCounter) &&
        Cleared(stats.frameLengthIncreaseCounter) && Cleared(stats.frameLengthDecreaseCounter) && Cleared(loss);
}

WebRtcAudioEncoder* Encoder() {
    static unsigned char storage[64];
    std::memset(storage, 0xC3, sizeof(storage));
    return reinterpret_cast<WebRtcAudioEncoder*>(storage);
}

bool EncoderUnchanged(const WebRtcAudioEncoder* encoder) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(encoder);
    for (int i = 0; i < 64; ++i)
        if (bytes[i] != 0xC3) return false;
    return true;
}

void TestAnaStats() {
    WebRtcAnaStats stats;
    Fill(stats);
    _ZN6webrtc8ANAStatsC1Ev_nid_postfix(&stats);
    Require(AllCleared(stats));

    WebRtcAudioEncoder* encoder = Encoder();
    WebRtcAnaStats result;
    Fill(result);
    Require(_ZNK6webrtc12AudioEncoder11GetANAStatsEv_nid_postfix(&result, encoder) == &result);
    Require(AllCleared(result));
    Require(EncoderUnchanged(encoder));
}

void TestDefaults() {
    WebRtcAudioEncoder* encoder = Encoder();
    Require(!_ZNK6webrtc12AudioEncoder6GetDtxEv_nid_postfix(encoder));
    _ZN6webrtc12AudioEncoder16SetTargetBitrateEi_nid_postfix(encoder, 32000);
    _ZN6webrtc12AudioEncoder26DisableAudioNetworkAdaptorEv_nid_postfix(encoder);
    _ZN6webrtc12AudioEncoder13OnReceivedRttEi_nid_postfix(encoder, 120);
    _ZN6webrtc12AudioEncoder18OnReceivedOverheadEm_nid_postfix(encoder, 40);
    _ZN6webrtc12AudioEncoder27SetReceiverFrameLengthRangeEii_nid_postfix(encoder, 20, 120);
    Require(!_ZNK6webrtc12AudioEncoder6GetDtxEv_nid_postfix(encoder));
    Require(EncoderUnchanged(encoder));
}

void TestNullPointers() {
    RequireInvalidArgument([] { _ZN6webrtc8ANAStatsC1Ev_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZNK6webrtc12AudioEncoder11GetANAStatsEv_nid_postfix(nullptr, Encoder()); });
}

}

int main() {
    TestAnaStats();
    TestDefaults();
    TestNullPointers();
    return 0;
}
