#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numbers>
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

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc resampler check failed at line %u\n", location.line());
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

double Tone(double frequency, double rate, std::size_t index) {
    return std::sin(2.0 * std::numbers::pi * frequency * static_cast<double>(index) / rate);
}

struct Fit {
    double amplitude;
    double residual;
};

template<typename TSample> Fit FitTone(const std::vector<TSample>& samples, std::size_t channels, std::size_t channel, std::size_t skip, double frequency, double rate) {
    double ss = 0, sc = 0, cc = 0, ys = 0, yc = 0;
    for (std::size_t i = skip; i < samples.size() / channels; ++i) {
        const double s = Tone(frequency, rate, i);
        const double c = std::cos(2.0 * std::numbers::pi * frequency * static_cast<double>(i) / rate);
        const double y = samples[i * channels + channel];
        ss += s * s;
        sc += s * c;
        cc += c * c;
        ys += y * s;
        yc += y * c;
    }
    const double determinant = ss * cc - sc * sc;
    const double a = (ys * cc - yc * sc) / determinant;
    const double b = (yc * ss - ys * sc) / determinant;
    double error = 0;
    std::size_t count = 0;
    for (std::size_t i = skip; i < samples.size() / channels; ++i) {
        const double model = a * Tone(frequency, rate, i) + b * std::cos(2.0 * std::numbers::pi * frequency * static_cast<double>(i) / rate);
        const double difference = samples[i * channels + channel] - model;
        error += difference * difference;
        ++count;
    }
    return {std::sqrt(a * a + b * b), std::sqrt(error / static_cast<double>(count))};
}

void RequireTone(const Fit& fit, double amplitude, std::source_location location = std::source_location::current()) {
    Require(std::fabs(fit.amplitude - amplitude) < amplitude * 0.01 && fit.residual < amplitude * 0.01, location);
}

std::vector<float> ResampleFloat(std::size_t sourceFrames, std::size_t destinationFrames, double sourceRate, double frequency, std::size_t chunks) {
    WebRtcPushSincResampler push;
    _ZN6webrtc17PushSincResamplerC1Emm_nid_postfix(&push, sourceFrames, destinationFrames);
    std::vector<float> output;
    std::vector<float> source(sourceFrames);
    std::vector<float> destination(destinationFrames);
    for (std::size_t chunk = 0; chunk < chunks; ++chunk) {
        for (std::size_t i = 0; i < sourceFrames; ++i) source[i] = static_cast<float>(1000.0 * Tone(frequency, sourceRate, chunk * sourceFrames + i));
        Require(_ZN6webrtc17PushSincResampler8ResampleEPKfmPfm_nid_postfix(&push, source.data(), sourceFrames, destination.data(), destinationFrames) == destinationFrames);
        output.insert(output.end(), destination.begin(), destination.end());
    }
    _ZN6webrtc17PushSincResamplerD1Ev_nid_postfix(&push);
    return output;
}

void TestPushSincResamplerKeepsTheTone() {
    RequireTone(FitTone(ResampleFloat(480, 160, 48000, 1000, 20), 1, 0, 320, 1000, 16000), 1000);
    RequireTone(FitTone(ResampleFloat(160, 480, 16000, 1000, 20), 1, 0, 960, 1000, 48000), 1000);
    RequireTone(FitTone(ResampleFloat(441, 480, 44100, 3000, 20), 1, 0, 960, 3000, 48000), 1000);
}

void RequireUpstreamSamples(std::size_t sourceFrames, std::size_t destinationFrames, const std::array<float, 6>& expected, std::source_location location = std::source_location::current()) {
    WebRtcPushSincResampler push;
    _ZN6webrtc17PushSincResamplerC1Emm_nid_postfix(&push, sourceFrames, destinationFrames);
    std::vector<float> source(sourceFrames), destination(destinationFrames);
    for (std::size_t chunk = 0; chunk < 4; ++chunk) {
        for (std::size_t i = 0; i < sourceFrames; ++i) {
            const double n = static_cast<double>(chunk * sourceFrames + i);
            source[i] = static_cast<float>(1000.0 * std::sin(n * 0.05) + 700.0 * std::sin(n * 0.9) + 300.0 * std::cos(n * 2.1));
        }
        _ZN6webrtc17PushSincResampler8ResampleEPKfmPfm_nid_postfix(&push, source.data(), sourceFrames, destination.data(), destinationFrames);
    }
    const std::array<std::size_t, 6> indices {0, 7, 33, 64, 101, 159};
    for (std::size_t i = 0; i < indices.size(); ++i) Require(std::fabs(destination[indices[i]] - expected[i]) < 0.01f, location);
    _ZN6webrtc17PushSincResamplerD1Ev_nid_postfix(&push);
}

void TestPushSincResamplerMatchesUpstream() {
    RequireUpstreamSamples(480, 160, {534.7039f, -281.8378f, 300.4824f, -448.5595f, -772.4929f, 676.2267f});
    RequireUpstreamSamples(160, 441, {-283.0425f, -1527.9186f, -166.0606f, -1441.7185f, -661.0906f, 168.9698f});
}

void TestPushSincResamplerRejectsBadSizes() {
    RequireInvalidArgument([] {
        WebRtcPushSincResampler push;
        _ZN6webrtc17PushSincResamplerC1Emm_nid_postfix(&push, 48, 160);
    });
    RequireInvalidArgument([] {
        WebRtcPushSincResampler push;
        _ZN6webrtc17PushSincResamplerC1Emm_nid_postfix(&push, 480, 0);
    });
    WebRtcPushSincResampler smallest;
    _ZN6webrtc17PushSincResamplerC1Emm_nid_postfix(&smallest, 49, 10);
    std::vector<float> few(49, 1.f), out(10);
    Require(_ZN6webrtc17PushSincResampler8ResampleEPKfmPfm_nid_postfix(&smallest, few.data(), 49, out.data(), 10) == 10);
    _ZN6webrtc17PushSincResamplerD1Ev_nid_postfix(&smallest);
    WebRtcPushSincResampler push;
    _ZN6webrtc17PushSincResamplerC1Emm_nid_postfix(&push, 480, 160);
    std::vector<float> source(480), destination(160);
    RequireInvalidArgument([&] { _ZN6webrtc17PushSincResampler8ResampleEPKfmPfm_nid_postfix(&push, source.data(), 479, destination.data(), 160); });
    RequireInvalidArgument([&] { _ZN6webrtc17PushSincResampler8ResampleEPKfmPfm_nid_postfix(&push, source.data(), 480, destination.data(), 159); });
    _ZN6webrtc17PushSincResamplerD1Ev_nid_postfix(&push);
    RequireInvalidArgument([] { _ZN6webrtc17PushSincResamplerC1Emm_nid_postfix(nullptr, 480, 160); });
}

void TestPushSincResamplerDeletingDestructor() {
    auto* push = static_cast<WebRtcPushSincResampler*>(_Znwm_nid_postfix(sizeof(WebRtcPushSincResampler)));
    _ZN6webrtc17PushSincResamplerC1Emm_nid_postfix(push, 160, 480);
    using Deleting = void (APS5_VABI*)(WebRtcPushSincResampler*);
    reinterpret_cast<Deleting>(static_cast<void* const*>(push->vtable)[1])(push);
    Require(lastFreed == push);
}

std::vector<std::int16_t> InterleavedTones(std::size_t frames, std::size_t channels, std::size_t chunk, double rate) {
    std::vector<std::int16_t> samples(frames * channels);
    for (std::size_t i = 0; i < frames; ++i) {
        for (std::size_t channel = 0; channel < channels; ++channel)
            samples[i * channels + channel] = static_cast<std::int16_t>(std::lround(8000.0 * Tone(1000.0 * static_cast<double>(channel + 1), rate, chunk * frames + i)));
    }
    return samples;
}

void TestPushResamplerStereo() {
    WebRtcPushResampler16 resampler;
    _ZN6webrtc13PushResamplerIsEC1Emmm_nid_postfix(&resampler, 480, 160, 2);
    std::vector<std::int16_t> output;
    std::vector<std::int16_t> destination(320);
    for (std::size_t chunk = 0; chunk < 20; ++chunk) {
        const std::vector<std::int16_t> source = InterleavedTones(480, 2, chunk, 48000);
        const int written = _ZN6webrtc13PushResamplerIsE8ResampleENS_15InterleavedViewIKsEENS2_IsEE_nid_postfix(
            &resampler, WebRtcInterleavedView {2, 480, source.data(), source.size()}, WebRtcMutableInterleavedView16 {2, 160, destination.data(), destination.size()});
        Require(written == 320);
        output.insert(output.end(), destination.begin(), destination.end());
    }
    RequireTone(FitTone(output, 2, 0, 320, 1000, 16000), 8000);
    RequireTone(FitTone(output, 2, 1, 320, 2000, 16000), 8000);
    std::vector<std::int16_t> wrongChannels(480);
    const std::vector<std::int16_t> source = InterleavedTones(480, 2, 0, 48000);
    RequireInvalidArgument([&] {
        _ZN6webrtc13PushResamplerIsE8ResampleENS_15InterleavedViewIKsEENS2_IsEE_nid_postfix(
            &resampler, WebRtcInterleavedView {2, 480, source.data(), source.size()}, WebRtcMutableInterleavedView16 {3, 160, wrongChannels.data(), wrongChannels.size()});
    });
    _ZN6webrtc13PushResamplerIsED1Ev_nid_postfix(&resampler);
    Require(resampler.source == nullptr && resampler.resamplersBegin == nullptr);
}

void TestPushResamplerSameRateCopies() {
    WebRtcPushResampler16 resampler;
    _ZN6webrtc13PushResamplerIsEC1Ev_nid_postfix(&resampler);
    const std::vector<std::int16_t> source = InterleavedTones(160, 2, 3, 16000);
    std::vector<std::int16_t> destination(320);
    Require(_ZN6webrtc13PushResamplerIsE8ResampleENS_15InterleavedViewIKsEENS2_IsEE_nid_postfix(
                &resampler, WebRtcInterleavedView {2, 160, source.data(), source.size()}, WebRtcMutableInterleavedView16 {2, 160, destination.data(), destination.size()}) == 320);
    Require(destination == source);
    Require(resampler.sourceView.numChannels == 2 && resampler.sourceView.samplesPerChannel == 160);
    std::vector<std::int16_t> mono(160);
    Require(_ZN6webrtc13PushResamplerIsE8ResampleEN3rtc9ArrayViewIKsLln4711EEENS3_IsLln4711EEE_nid_postfix(
                &resampler, WebRtcConstMonoView16 {source.data(), 160}, WebRtcMonoView16 {mono.data(), 160}) == 160);
    Require(std::memcmp(mono.data(), source.data(), 320) == 0);
    std::vector<std::int16_t> small(80);
    RequireInvalidArgument([&] {
        _ZN6webrtc13PushResamplerIsE8ResampleEN3rtc9ArrayViewIKsLln4711EEENS3_IsLln4711EEE_nid_postfix(
            &resampler, WebRtcConstMonoView16 {source.data(), 160}, WebRtcMonoView16 {small.data(), 80});
    });
    _ZN6webrtc13PushResamplerIsED1Ev_nid_postfix(&resampler);
}

void TestPushResamplerMonoMatchesTheFloatPath() {
    WebRtcPushResampler16 resampler;
    _ZN6webrtc13PushResamplerIsEC1Emmm_nid_postfix(&resampler, 160, 480, 1);
    WebRtcPushSincResampler push;
    _ZN6webrtc17PushSincResamplerC1Emm_nid_postfix(&push, 160, 480);
    std::vector<std::int16_t> destination(480);
    std::vector<float> floatSource(160), floatDestination(480);
    for (std::size_t chunk = 0; chunk < 6; ++chunk) {
        const std::vector<std::int16_t> source = InterleavedTones(160, 1, chunk, 16000);
        Require(_ZN6webrtc13PushResamplerIsE8ResampleEN3rtc9ArrayViewIKsLln4711EEENS3_IsLln4711EEE_nid_postfix(
                    &resampler, WebRtcConstMonoView16 {source.data(), 160}, WebRtcMonoView16 {destination.data(), 480}) == 480);
        for (std::size_t i = 0; i < 160; ++i) floatSource[i] = source[i];
        _ZN6webrtc17PushSincResampler8ResampleEPKfmPfm_nid_postfix(&push, floatSource.data(), 160, floatDestination.data(), 480);
        for (std::size_t i = 0; i < 480; ++i) {
            const float clamped = std::fmax(std::fmin(floatDestination[i], 32767.f), -32768.f);
            Require(destination[i] == static_cast<std::int16_t>(clamped + std::copysign(0.5f, clamped)));
        }
    }
    _ZN6webrtc17PushSincResamplerD1Ev_nid_postfix(&push);
    _ZN6webrtc13PushResamplerIsED1Ev_nid_postfix(&resampler);
    WebRtcPushResampler16 empty;
    _ZN6webrtc13PushResamplerIsEC1Ev_nid_postfix(&empty);
    RequireInvalidArgument([&] {
        _ZN6webrtc13PushResamplerIsE8ResampleEN3rtc9ArrayViewIKsLln4711EEENS3_IsLln4711EEE_nid_postfix(
            &empty, WebRtcConstMonoView16 {destination.data(), 160}, WebRtcMonoView16 {destination.data(), 480});
    });
    RequireInvalidArgument([] {
        WebRtcPushResampler16 zero;
        _ZN6webrtc13PushResamplerIsEC1Emmm_nid_postfix(&zero, 160, 480, 0);
    });
}

WebRtcAudioFrame* NewFrame(std::size_t samplesPerChannel, std::size_t channels, int rate) {
    auto* frame = new WebRtcAudioFrame;
    _ZN6webrtc10AudioFrameC1Ev_nid_postfix(frame);
    frame->samplesPerChannel = samplesPerChannel;
    frame->numChannels = channels;
    frame->sampleRateHz = rate;
    return frame;
}

void Remix(const std::vector<std::int16_t>& source, std::size_t channels, std::size_t frames, WebRtcPushResampler16& resampler, WebRtcAudioFrame* frame) {
    _ZN6webrtc3voe16RemixAndResampleENS_15InterleavedViewIKsEEiPNS_13PushResamplerIsEEPNS_10AudioFrameE_nid_postfix(
        WebRtcInterleavedView {channels, frames, source.data(), source.size()}, static_cast<int>(frames * 100), &resampler, frame);
}

void TestRemixDownmixesThenResamples() {
    WebRtcPushResampler16 resampler;
    _ZN6webrtc13PushResamplerIsEC1Ev_nid_postfix(&resampler);
    WebRtcAudioFrame* frame = NewFrame(160, 1, 16000);
    std::vector<std::int16_t> output;
    for (std::size_t chunk = 0; chunk < 20; ++chunk) {
        std::vector<std::int16_t> source(1440);
        for (std::size_t i = 0; i < 480; ++i) {
            const auto value = static_cast<std::int16_t>(std::lround(8000.0 * Tone(1000, 48000, chunk * 480 + i)));
            source[3 * i] = value;
            source[3 * i + 1] = static_cast<std::int16_t>(value / 2);
            source[3 * i + 2] = 0;
        }
        Remix(source, 3, 480, resampler, frame);
        Require(!frame->muted && frame->numChannels == 1 && frame->samplesPerChannel == 160);
        output.insert(output.end(), frame->data, frame->data + 160);
    }
    RequireTone(FitTone(output, 1, 0, 320, 1000, 16000), 4000);
    delete frame;
    _ZN6webrtc13PushResamplerIsED1Ev_nid_postfix(&resampler);
}

void TestRemixQuadToStereoAndUpmix() {
    WebRtcPushResampler16 resampler;
    _ZN6webrtc13PushResamplerIsEC1Ev_nid_postfix(&resampler);
    WebRtcAudioFrame* frame = NewFrame(160, 2, 16000);
    std::vector<std::int16_t> quad(640);
    for (std::size_t i = 0; i < 160; ++i) {
        quad[4 * i] = 10;
        quad[4 * i + 1] = 21;
        quad[4 * i + 2] = -7;
        quad[4 * i + 3] = -8;
    }
    Remix(quad, 4, 160, resampler, frame);
    Require(frame->numChannels == 2 && frame->data[0] == 15 && frame->data[1] == -8 && frame->data[318] == 15 && frame->data[319] == -8);

    WebRtcAudioFrame* stereo = NewFrame(160, 2, 16000);
    WebRtcPushResampler16 monoResampler;
    _ZN6webrtc13PushResamplerIsEC1Ev_nid_postfix(&monoResampler);
    std::vector<std::int16_t> mono(160);
    for (std::size_t i = 0; i < 160; ++i) mono[i] = static_cast<std::int16_t>(i * 3 - 200);
    Remix(mono, 1, 160, monoResampler, stereo);
    Require(stereo->numChannels == 2 && stereo->samplesPerChannel == 160);
    for (std::size_t i = 0; i < 160; ++i) Require(stereo->data[2 * i] == mono[i] && stereo->data[2 * i + 1] == mono[i]);

    WebRtcAudioFrame* target = NewFrame(160, 2, 16000);
    std::vector<std::int16_t> three(480);
    RequireInvalidArgument([&] { Remix(three, 3, 160, resampler, target); });
    delete frame;
    delete stereo;
    delete target;
    _ZN6webrtc13PushResamplerIsED1Ev_nid_postfix(&resampler);
    _ZN6webrtc13PushResamplerIsED1Ev_nid_postfix(&monoResampler);
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestPushSincResamplerKeepsTheTone();
    TestPushSincResamplerMatchesUpstream();
    TestPushSincResamplerRejectsBadSizes();
    TestPushSincResamplerDeletingDestructor();
    TestPushResamplerStereo();
    TestPushResamplerSameRateCopies();
    TestPushResamplerMonoMatchesTheFloatPath();
    TestRemixDownmixesThenResamples();
    TestRemixQuadToStereoAndUpmix();
    return 0;
}
