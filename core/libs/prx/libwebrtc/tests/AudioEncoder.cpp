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

struct UplinkCall {
    int calls = 0;
    const void* object = nullptr;
    int bitrateBps = 0;
    bool engaged = true;
    std::int64_t periodMs = 0;
};

UplinkCall g_first;
UplinkCall g_second;

void APS5_VABI RecordFirst(WebRtcAudioEncoder* encoder, int bitrateBps, WebRtcOptionalInt64 period) {
    g_first = {g_first.calls + 1, encoder, bitrateBps, period.engaged, period.value};
}

void APS5_VABI RecordSecond(WebRtcAudioEncoder* encoder, int bitrateBps, WebRtcOptionalInt64 period) {
    g_second = {g_second.calls + 1, encoder, bitrateBps, period.engaged, period.value};
}

struct FakeEncoder {
    WebRtcAudioEncoder base;
    const void* vtable[30];
    FakeEncoder(void (APS5_VABI* uplinkBandwidth)(WebRtcAudioEncoder*, int, WebRtcOptionalInt64)) : base{vtable}, vtable{} {
        vtable[21] = reinterpret_cast<const void*>(uplinkBandwidth);
    }
};

WebRtcBitrateAllocationUpdate Update(std::int64_t targetBps, std::int64_t bwePeriodUs) {
    return WebRtcBitrateAllocationUpdate{targetBps, 12345, 0.25, 70000, bwePeriodUs, 0.5};
}

void TestTargetAudioBitrate() {
    FakeEncoder first(RecordFirst);
    FakeEncoder second(RecordSecond);
    g_first = {};
    g_second = {};
    _ZN6webrtc12AudioEncoder28OnReceivedTargetAudioBitrateEi_nid_postfix(&first.base, 48000);
    Require(g_first.calls == 1 && g_second.calls == 0);
    Require(g_first.object == &first.base);
    Require(g_first.bitrateBps == 48000);
    Require(!g_first.engaged);
    _ZN6webrtc12AudioEncoder28OnReceivedTargetAudioBitrateEi_nid_postfix(&second.base, 16000);
    Require(g_first.calls == 1 && g_second.calls == 1);
    Require(g_second.object == &second.base && g_second.bitrateBps == 16000 && !g_second.engaged);
}

void TestUplinkAllocation() {
    FakeEncoder first(RecordFirst);
    FakeEncoder second(RecordSecond);
    g_first = {};
    g_second = {};
    _ZN6webrtc12AudioEncoder26OnReceivedUplinkAllocationENS_23BitrateAllocationUpdateE_nid_postfix(&first.base, Update(64000, 200000));
    Require(g_first.calls == 1 && g_second.calls == 0);
    Require(g_first.object == &first.base);
    Require(g_first.bitrateBps == 64000);
    Require(g_first.engaged);
    Require(g_first.periodMs == 200);
    _ZN6webrtc12AudioEncoder26OnReceivedUplinkAllocationENS_23BitrateAllocationUpdateE_nid_postfix(&second.base, Update(32000, 1500));
    Require(g_second.calls == 1 && g_first.calls == 1);
    Require(g_second.bitrateBps == 32000 && g_second.engaged && g_second.periodMs == 2);
}

void TestUplinkAllocationRounding() {
    FakeEncoder encoder(RecordFirst);
    struct Case {
        std::int64_t periodUs;
        std::int64_t periodMs;
    };
    const Case cases[] = {{0, 0}, {499, 0}, {500, 1}, {501, 1}, {1499, 1}, {1500, 2}, {-499, 0}, {-500, 0}, {-501, -1}, {-1500, -1}, {-1501, -2}};
    for (const Case& item : cases) {
        g_first = {};
        _ZN6webrtc12AudioEncoder26OnReceivedUplinkAllocationENS_23BitrateAllocationUpdateE_nid_postfix(&encoder.base, Update(1, item.periodUs));
        Require(g_first.calls == 1 && g_first.periodMs == item.periodMs);
    }
}

void TestNullPointers() {
    RequireInvalidArgument([] { _ZN6webrtc8ANAStatsC1Ev_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZNK6webrtc12AudioEncoder11GetANAStatsEv_nid_postfix(nullptr, Encoder()); });
}

}

int main() {
    TestAnaStats();
    TestDefaults();
    TestTargetAudioBitrate();
    TestUplinkAllocation();
    TestUplinkAllocationRounding();
    TestNullPointers();
    return 0;
}
