#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc AudioFrame check failed at line %u\n", location.line());
    std::abort();
}

template<typename TException, typename TAction> void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const TException&) {
        return;
    }
    Require(false, location);
}

std::unique_ptr<WebRtcAudioFrame> Filled() {
    auto frame = std::make_unique<WebRtcAudioFrame>();
    std::memset(frame.get(), 0x5A, sizeof(WebRtcAudioFrame));
    return frame;
}

bool AllSamples(const std::int16_t* data, std::int16_t value) {
    for (std::size_t i = 0; i < WebRtcAudioFrameMaxDataSizeSamples; ++i)
        if (data[i] != value) return false;
    return true;
}

void TestConstructor() {
    auto frame = Filled();
    _ZN6webrtc10AudioFrameC1Ev_nid_postfix(frame.get());
    Require(frame->timestamp == 0);
    Require(frame->elapsedTimeMs == -1);
    Require(frame->ntpTimeMs == -1);
    Require(frame->samplesPerChannel == 0);
    Require(frame->sampleRateHz == 0);
    Require(frame->numChannels == 0);
    Require(frame->speechType == 4);
    Require(frame->vadActivity == 2);
    Require(frame->profileTimestampMs == 0);
    Require(frame->packetInfos == nullptr);
    Require(frame->muted);
    Require(frame->channelLayout == 0);
    Require(!frame->absoluteCaptureTimestampMsEngaged);
    Require(AllSamples(frame->data, 0x5A5A));
    Require(_ZNK6webrtc10AudioFrame5mutedEv_nid_postfix(frame.get()));
}

void TestMutedData() {
    auto frame = Filled();
    _ZN6webrtc10AudioFrameC1Ev_nid_postfix(frame.get());
    const std::int16_t* zeroed = _ZNK6webrtc10AudioFrame4dataEv_nid_postfix(frame.get());
    Require(zeroed != frame->data);
    Require(AllSamples(zeroed, 0));
    Require(AllSamples(frame->data, 0x5A5A));

    std::int16_t* samples = _ZN6webrtc10AudioFrame12mutable_dataEv_nid_postfix(frame.get());
    Require(samples == frame->data);
    Require(!frame->muted);
    Require(AllSamples(frame->data, 0));
    Require(_ZNK6webrtc10AudioFrame4dataEv_nid_postfix(frame.get()) == frame->data);

    samples[0] = 1234;
    samples[WebRtcAudioFrameMaxDataSizeSamples - 1] = -5;
    Require(_ZN6webrtc10AudioFrame12mutable_dataEv_nid_postfix(frame.get()) == frame->data);
    Require(frame->data[0] == 1234);
    Require(frame->data[WebRtcAudioFrameMaxDataSizeSamples - 1] == -5);
}

void TestMute() {
    auto frame = Filled();
    _ZN6webrtc10AudioFrameC1Ev_nid_postfix(frame.get());
    _ZN6webrtc10AudioFrame12mutable_dataEv_nid_postfix(frame.get())[3] = 77;
    Require(!_ZNK6webrtc10AudioFrame5mutedEv_nid_postfix(frame.get()));
    _ZN6webrtc10AudioFrame4MuteEv_nid_postfix(frame.get());
    Require(_ZNK6webrtc10AudioFrame5mutedEv_nid_postfix(frame.get()));
    Require(frame->data[3] == 77);
    Require(AllSamples(_ZNK6webrtc10AudioFrame4dataEv_nid_postfix(frame.get()), 0));
    _ZN6webrtc10AudioFrame12mutable_dataEv_nid_postfix(frame.get());
    Require(frame->data[3] == 0);
}

void TestOperationsMute() {
    auto frame = Filled();
    _ZN6webrtc10AudioFrameC1Ev_nid_postfix(frame.get());
    frame->samplesPerChannel = 480;
    frame->numChannels = 2;
    _ZN6webrtc10AudioFrame12mutable_dataEv_nid_postfix(frame.get())[0] = 9;
    _ZN6webrtc20AudioFrameOperations4MuteEPNS_10AudioFrameE_nid_postfix(frame.get());
    Require(frame->muted);
    Require(frame->data[0] == 9);
    Require(frame->samplesPerChannel == 480);
    Require(frame->numChannels == 2);

    frame->muted = false;
    frame->samplesPerChannel = 960;
    frame->numChannels = 8;
    _ZN6webrtc20AudioFrameOperations4MuteEPNS_10AudioFrameE_nid_postfix(frame.get());
    Require(frame->muted);

    frame->muted = false;
    frame->samplesPerChannel = 7681;
    frame->numChannels = 0;
    _ZN6webrtc20AudioFrameOperations4MuteEPNS_10AudioFrameE_nid_postfix(frame.get());
    Require(frame->muted);

    frame->muted = false;
    frame->samplesPerChannel = 961;
    frame->numChannels = 8;
    RequireThrows<std::runtime_error>([&] { _ZN6webrtc20AudioFrameOperations4MuteEPNS_10AudioFrameE_nid_postfix(frame.get()); });
    Require(!frame->muted);
}

void TestNullFrame() {
    RequireThrows<std::invalid_argument>([] { _ZN6webrtc10AudioFrameC1Ev_nid_postfix(nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZN6webrtc10AudioFrame4MuteEv_nid_postfix(nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZNK6webrtc10AudioFrame5mutedEv_nid_postfix(nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZNK6webrtc10AudioFrame4dataEv_nid_postfix(nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZN6webrtc10AudioFrame12mutable_dataEv_nid_postfix(nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZN6webrtc20AudioFrameOperations4MuteEPNS_10AudioFrameE_nid_postfix(nullptr); });
}

}

void UpdateFrame(WebRtcAudioFrame* frame, std::uint32_t timestamp, const std::int16_t* data, std::size_t samplesPerChannel, std::int32_t sampleRateHz, std::int32_t speechType,
    std::int32_t vadActivity, std::size_t numChannels) {
    _ZN6webrtc10AudioFrame11UpdateFrameEjPKsmiNS0_10SpeechTypeENS0_11VADActivityEm_nid_postfix(frame, timestamp, data, samplesPerChannel, sampleRateHz, speechType, vadActivity,
        numChannels);
}

void TestUpdateFrame() {
    auto frame = Filled();
    std::int16_t samples[960];
    for (std::size_t i = 0; i < 960; ++i)
        samples[i] = static_cast<std::int16_t>(i * 7 - 3000);
    UpdateFrame(frame.get(), 0x12345678u, samples, 480, 48000, 0, 0, 2);
    Require(frame->timestamp == 0x12345678u);
    Require(frame->samplesPerChannel == 480);
    Require(frame->sampleRateHz == 48000);
    Require(frame->speechType == 0);
    Require(frame->vadActivity == 0);
    Require(frame->numChannels == 2);
    Require(frame->channelLayout == WebRtcChannelLayoutStereo);
    Require(!frame->muted);
    Require(std::memcmp(frame->data, samples, sizeof(samples)) == 0);
    Require(frame->data[960] == 0x5A5A);
    Require(frame->elapsedTimeMs == 0x5A5A5A5A5A5A5A5A && frame->ntpTimeMs == 0x5A5A5A5A5A5A5A5A);

    UpdateFrame(frame.get(), 1, nullptr, 160, 16000, 2, 1, 1);
    Require(frame->muted);
    Require(frame->samplesPerChannel == 160 && frame->numChannels == 1 && frame->channelLayout == WebRtcChannelLayoutMono);
    Require(std::memcmp(frame->data, samples, sizeof(samples)) == 0);
}

void TestUpdateFrameLayouts() {
    const std::int32_t layouts[] = {WebRtcChannelLayoutUnsupported, WebRtcChannelLayoutMono, WebRtcChannelLayoutStereo, WebRtcChannelLayoutSurround,
        WebRtcChannelLayoutQuad, WebRtcChannelLayout5_0, WebRtcChannelLayout5_1, WebRtcChannelLayout6_1, WebRtcChannelLayout7_1};
    auto frame = Filled();
    for (std::size_t channels = 0; channels <= 8; ++channels) {
        UpdateFrame(frame.get(), 0, nullptr, 10, 8000, 4, 2, channels);
        Require(frame->channelLayout == layouts[channels]);
    }
}

void TestUpdateFrameLimits() {
    auto frame = Filled();
    static std::int16_t samples[WebRtcAudioFrameMaxDataSizeSamples];
    UpdateFrame(frame.get(), 0, samples, 960, 48000, 0, 0, 8);
    Require(!frame->muted && frame->numChannels == 8);
    RequireThrows<std::invalid_argument>([&] { UpdateFrame(frame.get(), 0, samples, 10, 48000, 0, 0, 9); });
    Require(frame->numChannels == 8);
    RequireThrows<std::invalid_argument>([&] { UpdateFrame(frame.get(), 0, samples, 961, 48000, 0, 0, 8); });
    RequireThrows<std::invalid_argument>([&] { UpdateFrame(nullptr, 0, samples, 10, 48000, 0, 0, 1); });
}

void TestDataView() {
    auto frame = Filled();
    frame->muted = false;
    frame->samplesPerChannel = 441;
    frame->numChannels = 2;
    WebRtcInterleavedView view = _ZNK6webrtc10AudioFrame9data_viewEv_nid_postfix(frame.get());
    Require(view.numChannels == 2 && view.samplesPerChannel == 441 && view.size == 882 && view.data == frame->data);

    frame->muted = true;
    view = _ZNK6webrtc10AudioFrame9data_viewEv_nid_postfix(frame.get());
    Require(view.data == _ZNK6webrtc10AudioFrame4dataEv_nid_postfix(frame.get()) && view.data != frame->data && view.size == 882);
    Require(AllSamples(view.data, 0));

    frame->samplesPerChannel = 0;
    view = _ZNK6webrtc10AudioFrame9data_viewEv_nid_postfix(frame.get());
    Require(view.data == nullptr && view.size == 0 && view.numChannels == 2 && view.samplesPerChannel == 0);
    frame->muted = false;
    frame->numChannels = 0;
    frame->samplesPerChannel = 5;
    view = _ZNK6webrtc10AudioFrame9data_viewEv_nid_postfix(frame.get());
    Require(view.data == nullptr && view.size == 0 && view.samplesPerChannel == 5);
    RequireThrows<std::invalid_argument>([] { _ZNK6webrtc10AudioFrame9data_viewEv_nid_postfix(nullptr); });
}

void TestCopyFrom() {
    auto source = Filled();
    auto destination = Filled();
    _ZN6webrtc10AudioFrameC1Ev_nid_postfix(source.get());
    _ZN6webrtc10AudioFrameC1Ev_nid_postfix(destination.get());
    source->timestamp = 0x12345678;
    source->elapsedTimeMs = 111;
    source->ntpTimeMs = 222;
    source->samplesPerChannel = 2;
    source->sampleRateHz = 48000;
    source->numChannels = 2;
    source->speechType = 3;
    source->vadActivity = 1;
    source->profileTimestampMs = 444;
    source->channelLayout = WebRtcChannelLayoutStereo;
    source->absoluteCaptureTimestampMs.value = 987654321;
    source->absoluteCaptureTimestampMsEngaged = true;
    source->muted = false;
    source->data[0] = -1;
    source->data[1] = 22;
    source->data[2] = 333;
    source->data[3] = -4444;
    destination->profileTimestampMs = 999;

    _ZN6webrtc10AudioFrame8CopyFromERKS0__nid_postfix(destination.get(), source.get());
    Require(destination->timestamp == source->timestamp);
    Require(destination->elapsedTimeMs == source->elapsedTimeMs && destination->ntpTimeMs == source->ntpTimeMs);
    Require(destination->samplesPerChannel == 2 && destination->sampleRateHz == 48000 && destination->numChannels == 2);
    Require(destination->speechType == 3 && destination->vadActivity == 1 && destination->channelLayout == WebRtcChannelLayoutStereo);
    Require(destination->absoluteCaptureTimestampMsEngaged && destination->absoluteCaptureTimestampMs.value == 987654321);
    Require(!destination->muted && std::memcmp(destination->data, source->data, 4 * sizeof(source->data[0])) == 0);
    for (std::size_t index = 4; index < WebRtcAudioFrameMaxDataSizeSamples; ++index) Require(destination->data[index] == 0);
    Require(destination->profileTimestampMs == 999 && destination->packetInfos == nullptr);

    source->muted = true;
    destination->muted = false;
    destination->data[0] = 123;
    _ZN6webrtc10AudioFrame8CopyFromERKS0__nid_postfix(destination.get(), source.get());
    Require(destination->muted && destination->timestamp == source->timestamp);
    Require(AllSamples(_ZNK6webrtc10AudioFrame4dataEv_nid_postfix(destination.get()), 0));
}

void TestCopyFromOversizedSource() {
    auto source = Filled();
    _ZN6webrtc10AudioFrameC1Ev_nid_postfix(source.get());
    source->samplesPerChannel = WebRtcAudioFrameMaxDataSizeSamples;
    source->numChannels = 2;
    auto destination = Filled();
    _ZN6webrtc10AudioFrameC1Ev_nid_postfix(destination.get());
    RequireThrows<std::runtime_error>([&] { _ZN6webrtc10AudioFrame8CopyFromERKS0__nid_postfix(destination.get(), source.get()); });
}

int main() {
    TestConstructor();
    TestMutedData();
    TestMute();
    TestOperationsMute();
    TestNullFrame();
    TestUpdateFrame();
    TestUpdateFrameLayouts();
    TestUpdateFrameLimits();
    TestDataView();
    TestCopyFrom();
    TestCopyFromOversizedSource();
    return 0;
}
