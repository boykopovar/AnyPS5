#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "Testing.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

void* lastFreed = nullptr;

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) { lastFreed = pointer; std::free(pointer); }
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

struct Combiner {
    std::unique_ptr<WebRtcFrameCombiner> object = std::make_unique<WebRtcFrameCombiner>();
    explicit Combiner(bool useLimiter) { _ZN6webrtc13FrameCombinerC1Eb_nid_postfix(object.get(), useLimiter); }
    ~Combiner() { _ZN6webrtc13FrameCombinerD1Ev_nid_postfix(object.get()); }
    Combiner(const Combiner&) = delete;
    Combiner& operator=(const Combiner&) = delete;
    void Combine(const std::vector<WebRtcAudioFrame*>& frames, std::size_t channels, int rate, std::size_t streams, WebRtcAudioFrame* output) {
        _ZN6webrtc13FrameCombiner7CombineEN3rtc9ArrayViewIKPNS_10AudioFrameELln4711EEEmimS4__nid_postfix(
            object.get(), {frames.empty() ? nullptr : frames.data(), frames.size()}, channels, rate, streams, output);
    }
};

std::unique_ptr<WebRtcAudioFrame> Frame() {
    auto frame = std::make_unique<WebRtcAudioFrame>();
    _ZN6webrtc10AudioFrameC1Ev_nid_postfix(frame.get());
    return frame;
}

std::unique_ptr<WebRtcAudioFrame> Filled(int rate, std::size_t channels, std::int16_t value) {
    auto frame = Frame();
    const std::size_t samples = static_cast<std::size_t>(rate / 100);
    std::vector<std::int16_t> data(samples * channels, value);
    _ZN6webrtc10AudioFrame11UpdateFrameEjPKsmiNS0_10SpeechTypeENS0_11VADActivityEm_nid_postfix(
        frame.get(), 0, data.data(), samples, rate, WebRtcAudioFrameSpeechUndefined, WebRtcAudioFrameVadUnknown, channels);
    return frame;
}

void TestConstructorAndDestructor() {
    auto object = std::make_unique<WebRtcFrameCombiner>();
    std::memset(static_cast<void*>(object.get()), 0x5a, sizeof(*object));
    _ZN6webrtc13FrameCombinerC1Eb_nid_postfix(object.get(), true);
    const WebRtcFrameCombiner& c = *object;
    void* dumper = c.dataDumper;
    Require(dumper != nullptr && c.useLimiter);
    Require(c.limiter.apmDataDumper == dumper && c.limiter.levelEstimator.apmDataDumper == dumper && c.limiter.interpGainCurve.apmDataDumper == dumper);
    Require(c.limiter.interpGainCurve.identityHistogram == nullptr && c.limiter.interpGainCurve.saturationHistogram == nullptr);
    Require(!c.limiter.interpGainCurve.stats.available && c.limiter.interpGainCurve.stats.region == 0 && c.limiter.interpGainCurve.stats.regionDurationFrames == 0);
    Require(c.limiter.interpGainCurve.stats.lookUpsIdentityRegion == 0 && c.limiter.interpGainCurve.stats.lookUpsSaturationRegion == 0);
    Require(c.limiter.levelEstimator.filterStateLevel == 0.0f);
    Require(c.limiter.levelEstimator.samplesInFrame == 480 && c.limiter.levelEstimator.samplesInSubFrame == 24);
    Require(c.limiter.lastScalingFactor == 1.0f && c.limiter.scalingFactors[0] == 0.0f && c.limiter.perSampleScalingFactors[479] == 0.0f);
    Require(c.mixingBuffer[0] == 0.0f && c.mixingBuffer[3839] == 0.0f);
    _ZN6webrtc13FrameCombinerD1Ev_nid_postfix(object.get());
    Require(lastFreed == dumper);
    _ZN6webrtc13FrameCombinerC1Eb_nid_postfix(object.get(), false);
    Require(!object->useLimiter);
    _ZN6webrtc13FrameCombinerD1Ev_nid_postfix(object.get());
}

void TestZeroFramesProduceSilence() {
    Combiner combiner(true);
    auto output = Filled(48000, 2, 1000);
    output->timestamp = 77;
    combiner.Combine({}, 2, 48000, 0, output.get());
    Require(output->muted && output->samplesPerChannel == 480 && output->numChannels == 2 && output->sampleRateHz == 48000);
    Require(output->timestamp == 0 && output->elapsedTimeMs == -1);
    Require(output->speechType == WebRtcAudioFrameSpeechUndefined && output->vadActivity == WebRtcAudioFrameVadUnknown);
    Require(output->channelLayout == WebRtcChannelLayoutStereo);
}

void TestOneStreamCopiesTheFrame() {
    Combiner combiner(true);
    auto input = Frame();
    std::array<std::int16_t, 320> data{};
    for (std::size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<std::int16_t>(i * 97 - 15000);
    _ZN6webrtc10AudioFrame11UpdateFrameEjPKsmiNS0_10SpeechTypeENS0_11VADActivityEm_nid_postfix(input.get(), 1234, data.data(), 160, 16000, 0, 1, 2);
    input->elapsedTimeMs = 50;
    input->ntpTimeMs = 9000;
    auto output = Frame();
    combiner.Combine({input.get()}, 2, 16000, 1, output.get());
    Require(!output->muted && output->samplesPerChannel == 160 && output->numChannels == 2 && output->sampleRateHz == 16000);
    Require(output->timestamp == 1234 && output->elapsedTimeMs == 50 && output->ntpTimeMs == 9000);
    Require(std::memcmp(output->data, data.data(), sizeof(data)) == 0);
    Require(combiner.object->mixingBuffer[0] == 0.0f);
}

void TestOneStreamUpmixesMonoInPlace() {
    Combiner combiner(false);
    auto input = Frame();
    const std::array<std::int16_t, 160> data = [] {
        std::array<std::int16_t, 160> values{};
        for (std::size_t i = 0; i < values.size(); ++i)
            values[i] = static_cast<std::int16_t>(i + 1);
        return values;
    }();
    _ZN6webrtc10AudioFrame11UpdateFrameEjPKsmiNS0_10SpeechTypeENS0_11VADActivityEm_nid_postfix(input.get(), 0, data.data(), 160, 16000, 0, 1, 1);
    auto output = Frame();
    combiner.Combine({input.get()}, 2, 16000, 1, output.get());
    Require(input->numChannels == 2 && output->numChannels == 2 && output->samplesPerChannel == 160);
    for (std::size_t i = 0; i < 160; ++i)
        Require(input->data[2 * i] == data[i] && input->data[2 * i + 1] == data[i] && output->data[2 * i] == data[i] && output->data[2 * i + 1] == data[i]);
}

void TestTwoStreamsSumAndSaturateWithoutLimiter() {
    Combiner combiner(false);
    auto first = Filled(48000, 2, 0);
    auto second = Filled(48000, 2, 0);
    for (std::size_t i = 0; i < 480; ++i) {
        first->data[2 * i] = 20000;
        second->data[2 * i] = 20000;
        first->data[2 * i + 1] = static_cast<std::int16_t>(i % 2 ? -20000 : 100);
        second->data[2 * i + 1] = static_cast<std::int16_t>(i % 2 ? -20000 : 200);
    }
    first->timestamp = 500;
    second->timestamp = 300;
    first->ntpTimeMs = 10;
    second->ntpTimeMs = 20;
    first->elapsedTimeMs = 7;
    second->elapsedTimeMs = 9;
    auto output = Frame();
    combiner.Combine({first.get(), second.get()}, 2, 48000, 2, output.get());
    Require(!output->muted && output->samplesPerChannel == 480 && output->numChannels == 2);
    Require(output->timestamp == 300 && output->ntpTimeMs == 10 && output->elapsedTimeMs == 9);
    for (std::size_t i = 0; i < 480; ++i) {
        Require(output->data[2 * i] == 32767);
        Require(output->data[2 * i + 1] == (i % 2 ? -32768 : 300));
    }
    Require(combiner.object->limiter.lastScalingFactor == 1.0f && !combiner.object->limiter.interpGainCurve.stats.available);
}

void TestStereoFramesDownmixToMonoByTruncatingAverage() {
    Combiner combiner(false);
    auto first = Filled(8000, 2, 0);
    auto second = Frame();
    _ZN6webrtc10AudioFrame11UpdateFrameEjPKsmiNS0_10SpeechTypeENS0_11VADActivityEm_nid_postfix(second.get(), 0, nullptr, 80, 8000, 0, 1, 1);
    for (std::size_t i = 0; i < 80; ++i) {
        first->data[2 * i] = static_cast<std::int16_t>(i % 2 ? -3 : 3);
        first->data[2 * i + 1] = static_cast<std::int16_t>(i % 2 ? -4 : 4);
    }
    auto output = Frame();
    combiner.Combine({first.get(), second.get()}, 1, 8000, 2, output.get());
    Require(first->numChannels == 1 && second->numChannels == 1 && second->muted);
    Require(output->numChannels == 1 && output->samplesPerChannel == 80);
    for (std::size_t i = 0; i < 80; ++i)
        Require(first->data[i] == (i % 2 ? -3 : 3) && output->data[i] == (i % 2 ? -3 : 3));
}

void TestLimiterInTheKneeRegion() {
    Combiner combiner(true);
    auto first = Filled(48000, 1, 15500);
    auto second = Filled(48000, 1, 15500);
    auto output = Frame();
    const float gain = -7.353257842623861507e-06f * 31000.0f + 1.224629044532775879f;
    const float firstSubframe = 1.0f * (1.0f - gain) + gain;
    const std::int16_t limited = static_cast<std::int16_t>(31000.0f * gain + 0.5f);
    combiner.Combine({first.get(), second.get()}, 1, 48000, 2, output.get());
    const WebRtcLimiter& limiter = combiner.object->limiter;
    Require(gain < 1.0f && limited < 31000);
    Require(limiter.levelEstimator.filterStateLevel == 31000.0f);
    Require(limiter.lastScalingFactor == gain && limiter.scalingFactors[0] == 1.0f && limiter.scalingFactors[20] == gain);
    Require(limiter.interpGainCurve.stats.available && limiter.interpGainCurve.stats.lookUpsKneeRegion == 20);
    Require(limiter.interpGainCurve.stats.lookUpsIdentityRegion == 0 && limiter.interpGainCurve.stats.region == 1);
    Require(limiter.interpGainCurve.stats.regionDurationFrames == 19);
    const std::int16_t unscaled = static_cast<std::int16_t>(31000.0f * firstSubframe + 0.5f);
    for (std::size_t i = 0; i < 24; ++i)
        Require(output->data[i] == unscaled);
    for (std::size_t i = 24; i < 480; ++i)
        Require(output->data[i] == limited);
    combiner.Combine({first.get(), second.get()}, 1, 48000, 2, output.get());
    for (std::size_t i = 0; i < 480; ++i)
        Require(output->data[i] == limited);
    Require(limiter.interpGainCurve.stats.lookUpsKneeRegion == 40 && limiter.interpGainCurve.stats.regionDurationFrames == 39);
}

void TestHighRatesMixOnly480SamplesPerChannel() {
    Combiner combiner(false);
    auto first = Filled(96000, 2, 1);
    auto second = Filled(96000, 2, 2);
    auto output = Frame();
    combiner.Combine({first.get(), second.get()}, 2, 96000, 2, output.get());
    Require(output->sampleRateHz == 96000 && output->samplesPerChannel == 480 && output->numChannels == 2);
    for (std::size_t i = 0; i < 960; ++i)
        Require(output->data[i] == 3);
}

void TestChannelCountIsCappedAtEight() {
    Combiner combiner(false);
    auto output = Frame();
    combiner.Combine({}, 12, 16000, 0, output.get());
    Require(output->numChannels == 8 && output->samplesPerChannel == 160 && output->channelLayout == WebRtcChannelLayout7_1);
}

void TestNonMultipleOf20WorksWithoutLimiter() {
    Combiner combiner(false);
    auto first = Filled(44100, 1, 5);
    auto second = Filled(44100, 1, 6);
    auto output = Frame();
    combiner.Combine({first.get(), second.get()}, 1, 44100, 2, output.get());
    Require(output->samplesPerChannel == 441 && output->data[0] == 11 && output->data[440] == 11);
}

void TestRejectsUpstreamPreconditions() {
    Combiner combiner(true);
    auto first = Filled(44100, 1, 5);
    auto second = Filled(44100, 1, 6);
    auto output = Frame();
    ExpectErr<std::invalid_argument>([&] { combiner.Combine({first.get(), second.get()}, 1, 44100, 2, output.get()); });
    ExpectErr<std::invalid_argument>([&] { combiner.Combine({}, 2, -1, 0, output.get()); });
    ExpectErr<std::invalid_argument>([&] { combiner.Combine({}, 8, 192000, 0, output.get()); });
    ExpectErr<std::invalid_argument>([&] { combiner.Combine({}, 2, 48000, 0, nullptr); });
    ExpectErr<std::invalid_argument>([] { _ZN6webrtc13FrameCombinerC1Eb_nid_postfix(nullptr, true); });
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestConstructorAndDestructor();
    TestZeroFramesProduceSilence();
    TestOneStreamCopiesTheFrame();
    TestOneStreamUpmixesMonoInPlace();
    TestTwoStreamsSumAndSaturateWithoutLimiter();
    TestStereoFramesDownmixToMonoByTruncatingAverage();
    TestLimiterInTheKneeRegion();
    TestHighRatesMixOnly480SamplesPerChannel();
    TestChannelCountIsCappedAtEight();
    TestNonMultipleOf20WorksWithoutLimiter();
    TestRejectsUpstreamPreconditions();
    return 0;
}
