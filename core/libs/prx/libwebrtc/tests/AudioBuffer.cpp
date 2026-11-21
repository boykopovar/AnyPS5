#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

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
    std::fprintf(stderr, "libwebrtc AudioBuffer check failed at line %u\n", location.line());
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

using Destructor = void (APS5_VABI*)(WebRtcAudioBuffer*);

Destructor Slot(const WebRtcAudioBuffer& buffer, int slot) {
    return reinterpret_cast<Destructor>(static_cast<void* const*>(buffer.vtable)[slot]);
}

struct Buffer {
    WebRtcAudioBuffer object{};
    Buffer(std::size_t inputRate, std::size_t inputChannels, std::size_t bufferRate, std::size_t bufferChannels, std::size_t outputRate, std::size_t outputChannels) {
        _ZN6webrtc11AudioBufferC1Emmmmmm_nid_postfix(&object, inputRate, inputChannels, bufferRate, bufferChannels, outputRate, outputChannels);
    }
    ~Buffer() { Slot(object, 0)(&object); }
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
};

WebRtcStreamConfig Config(int rate, std::size_t channels) {
    return {rate, channels, static_cast<std::size_t>(rate / 100)};
}

void TestMonoWithoutResamplingOrBands() {
    Buffer buffer(16000, 1, 16000, 1, 16000, 1);
    const auto& b = buffer.object;
    Require(b.vtable != nullptr);
    Require(b.inputNumFrames == 160 && b.bufferNumFrames == 160 && b.outputNumFrames == 160);
    Require(b.inputNumChannels == 1 && b.bufferNumChannels == 1 && b.outputNumChannels == 0);
    Require(b.numChannels == 1 && b.numBands == 1 && b.numSplitFrames == 160);
    Require(b.splitData == nullptr && b.splittingFilter == nullptr);
    Require(b.inputResamplers.begin == nullptr && b.outputResamplers.begin == nullptr);
    Require(b.downmixByAveraging && b.channelForDownmixing == 0);
    const auto* data = b.data;
    Require(data->numFrames == 160 && data->numFramesPerBand == 160 && data->numChannels == 1 && data->numBands == 1);
    Require(data->channels[0] == data->data && data->bands[0] == data->data);
    Require(data->channelsView.end - data->channelsView.begin == 1 && data->bandsView.end - data->bandsView.begin == 1);
    Require(data->channelsView.begin[0].begin[0].data == data->data && data->channelsView.begin[0].begin[0].size == 160);
    for (std::size_t i = 0; i < 160; ++i)
        Require(data->data[i] == 0.f);
}

void TestThreeBandsAt48kHz() {
    Buffer buffer(48000, 2, 48000, 2, 48000, 2);
    const auto& b = buffer.object;
    Require(b.numBands == 3 && b.numSplitFrames == 160);
    const auto* split = b.splitData;
    Require(split != nullptr && split->numFrames == 480 && split->numFramesPerBand == 160 && split->numBands == 3 && split->numChannels == 2);
    for (std::size_t channel = 0; channel < 2; ++channel) {
        for (std::size_t band = 0; band < 3; ++band) {
            float* expected = split->data + channel * 480 + band * 160;
            Require(split->bands[channel * 3 + band] == expected);
            Require(split->channels[band * 2 + channel] == expected);
            Require(split->bandsView.begin[channel].begin[band].data == expected && split->bandsView.begin[channel].begin[band].size == 160);
            Require(split->channelsView.begin[band].begin[channel].data == expected);
        }
    }
    Require(split->bandsView.end - split->bandsView.begin == 2 && split->channelsView.end - split->channelsView.begin == 3);
    const auto* filter = b.splittingFilter;
    Require(filter->numBands == 3 && filter->twoBandsBegin == nullptr && filter->threeBandsEnd - filter->threeBandsBegin == 2);
    const auto* bytes = reinterpret_cast<const unsigned char*>(filter->threeBandsBegin);
    for (std::size_t i = 0; i < 2 * sizeof(WebRtcThreeBandFilterBank); ++i)
        Require(bytes[i] == 0);
}

void TestTwoBandsAt32kHz() {
    Buffer buffer(32000, 1, 32000, 1, 32000, 1);
    const auto& b = buffer.object;
    Require(b.numBands == 2 && b.numSplitFrames == 160);
    Require(b.splittingFilter->numBands == 2 && b.splittingFilter->twoBandsEnd - b.splittingFilter->twoBandsBegin == 1);
    Require(b.splittingFilter->threeBandsBegin == nullptr);
    Require(b.splitData->bands[1] == b.splitData->data + 160);
}

void TestResamplersPerBufferChannel() {
    Buffer buffer(44100, 4, 48000, 2, 16000, 2);
    const auto& b = buffer.object;
    Require(b.inputNumFrames == 441 && b.bufferNumFrames == 480 && b.outputNumFrames == 160);
    Require(b.inputResamplers.end - b.inputResamplers.begin == 2 && b.outputResamplers.end - b.outputResamplers.begin == 2);
    Require(b.inputResamplers.begin[0]->destinationFrames == 480 && b.outputResamplers.begin[1]->destinationFrames == 160);
}

void TestStereoRoundTrip() {
    Buffer buffer(16000, 2, 16000, 2, 16000, 2);
    std::vector<std::int16_t> input(320);
    for (std::size_t i = 0; i < input.size(); ++i)
        input[i] = static_cast<std::int16_t>((i % 2 ? -1 : 1) * static_cast<int>(i * 97 % 30000));
    const auto config = Config(16000, 2);
    _ZN6webrtc11AudioBuffer8CopyFromEPKsRKNS_12StreamConfigE_nid_postfix(&buffer.object, input.data(), &config);
    for (std::size_t j = 0; j < 160; ++j) {
        Require(buffer.object.data->channels[0][j] == input[2 * j]);
        Require(buffer.object.data->channels[1][j] == input[2 * j + 1]);
    }
    std::vector<std::int16_t> output(320, 0x5555);
    _ZN6webrtc11AudioBuffer6CopyToERKNS_12StreamConfigEPs_nid_postfix(&buffer.object, &config, output.data());
    Require(output == input);
}

void TestDownmixByAveragingTruncates() {
    Buffer buffer(8000, 2, 8000, 1, 8000, 1);
    std::vector<std::int16_t> input(160);
    for (std::size_t j = 0; j < 80; ++j) {
        input[2 * j] = static_cast<std::int16_t>(j % 2 ? -3 : 3);
        input[2 * j + 1] = static_cast<std::int16_t>(j % 2 ? -4 : 4);
    }
    const auto config = Config(8000, 2);
    _ZN6webrtc11AudioBuffer8CopyFromEPKsRKNS_12StreamConfigE_nid_postfix(&buffer.object, input.data(), &config);
    for (std::size_t j = 0; j < 80; ++j)
        Require(buffer.object.data->channels[0][j] == (j % 2 ? -3.f : 3.f));
}

void TestMonoBufferFillsEveryOutputChannel() {
    Buffer buffer(16000, 1, 16000, 1, 16000, 1);
    float* samples = buffer.object.data->channels[0];
    for (std::size_t j = 0; j < 160; ++j)
        samples[j] = static_cast<float>(j);
    samples[0] = 40000.f;
    samples[1] = -40000.f;
    samples[2] = 1.5f;
    samples[3] = -1.5f;
    samples[4] = 0.49f;
    const auto config = Config(16000, 3);
    std::vector<std::int16_t> output(480);
    _ZN6webrtc11AudioBuffer6CopyToERKNS_12StreamConfigEPs_nid_postfix(&buffer.object, &config, output.data());
    const std::array<std::int16_t, 5> expected{32767, -32768, 2, -2, 0};
    for (std::size_t j = 0; j < 160; ++j) {
        const std::int16_t value = j < expected.size() ? expected[j] : static_cast<std::int16_t>(j);
        for (std::size_t c = 0; c < 3; ++c)
            Require(output[3 * j + c] == value);
    }
}

void TestResamplingMatchesPushSincResampler() {
    Buffer buffer(32000, 1, 48000, 1, 48000, 1);
    WebRtcPushSincResampler reference{};
    _ZN6webrtc17PushSincResamplerC1Emm_nid_postfix(&reference, 320, 480);
    const auto config = Config(32000, 1);
    std::vector<std::int16_t> input(320);
    std::vector<float> floats(320), expected(480);
    for (int frame = 0; frame < 3; ++frame) {
        for (std::size_t i = 0; i < 320; ++i) {
            const double t = static_cast<double>(frame * 320 + i) / 32000.0;
            input[i] = static_cast<std::int16_t>(std::lround(12000.0 * std::sin(2.0 * std::numbers::pi * 1000.0 * t)));
            floats[i] = input[i];
        }
        _ZN6webrtc11AudioBuffer8CopyFromEPKsRKNS_12StreamConfigE_nid_postfix(&buffer.object, input.data(), &config);
        _ZN6webrtc17PushSincResampler8ResampleEPKfmPfm_nid_postfix(&reference, floats.data(), 320, expected.data(), 480);
        Require(std::memcmp(buffer.object.data->channels[0], expected.data(), 480 * sizeof(float)) == 0);
    }
    _ZN6webrtc17PushSincResamplerD1Ev_nid_postfix(&reference);
}

void TestDeletingDestructorFreesTheObject() {
    auto* storage = static_cast<WebRtcAudioBuffer*>(_Znwm_nid_postfix(sizeof(WebRtcAudioBuffer)));
    _ZN6webrtc11AudioBufferC1Emmmmmm_nid_postfix(storage, 48000, 1, 48000, 1, 44100, 1);
    Require(storage->outputResamplers.end - storage->outputResamplers.begin == 1);
    lastFreed = nullptr;
    Slot(*storage, 1)(storage);
    Require(lastFreed == storage);
}

void TestRejectsUpstreamPreconditions() {
    WebRtcAudioBuffer object{};
    RequireInvalidArgument([&] { _ZN6webrtc11AudioBufferC1Emmmmmm_nid_postfix(nullptr, 16000, 1, 16000, 1, 16000, 1); });
    RequireInvalidArgument([&] { _ZN6webrtc11AudioBufferC1Emmmmmm_nid_postfix(&object, 0, 1, 16000, 1, 16000, 1); });
    RequireInvalidArgument([&] { _ZN6webrtc11AudioBufferC1Emmmmmm_nid_postfix(&object, 16000, 0, 16000, 1, 16000, 1); });
    RequireInvalidArgument([&] { _ZN6webrtc11AudioBufferC1Emmmmmm_nid_postfix(&object, 16000, 1, 16000, 2, 16000, 2); });
    Buffer buffer(16000, 2, 16000, 2, 16000, 2);
    std::vector<std::int16_t> samples(320);
    const auto wrongChannels = Config(16000, 1);
    const auto wrongRate = Config(8000, 2);
    RequireInvalidArgument([&] { _ZN6webrtc11AudioBuffer8CopyFromEPKsRKNS_12StreamConfigE_nid_postfix(&buffer.object, samples.data(), &wrongChannels); });
    RequireInvalidArgument([&] { _ZN6webrtc11AudioBuffer8CopyFromEPKsRKNS_12StreamConfigE_nid_postfix(&buffer.object, samples.data(), &wrongRate); });
    RequireInvalidArgument([&] { _ZN6webrtc11AudioBuffer6CopyToERKNS_12StreamConfigEPs_nid_postfix(&buffer.object, &wrongChannels, samples.data()); });
}

void TestGuardedPointersThrow() {
#ifdef _WIN32
    SYSTEM_INFO sys{};
    GetSystemInfo(&sys);
    const std::size_t pageSize = sys.dwPageSize;
    void* page = VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Require(page != nullptr);
    DWORD oldProtect = 0;
    Require(VirtualProtect(page, pageSize, PAGE_READONLY, &oldProtect) != 0);
#else
    const std::size_t pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    void* page = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(page != MAP_FAILED);
    Require(mprotect(page, pageSize, PROT_READ) == 0);
#endif
    RequireInvalidArgument([&] { _ZN6webrtc11AudioBufferC1Emmmmmm_nid_postfix(static_cast<WebRtcAudioBuffer*>(page), 48000, 2, 48000, 2, 48000, 2); });
#ifdef _WIN32
    VirtualFree(page, 0, MEM_RELEASE);
#else
    munmap(page, pageSize);
#endif
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestMonoWithoutResamplingOrBands();
    TestThreeBandsAt48kHz();
    TestTwoBandsAt32kHz();
    TestResamplersPerBufferChannel();
    TestStereoRoundTrip();
    TestDownmixByAveragingTruncates();
    TestMonoBufferFillsEveryOutputChannel();
    TestResamplingMatchesPushSincResampler();
    TestDeletingDestructorFreesTheObject();
    TestRejectsUpstreamPreconditions();
    TestGuardedPointersThrow();
    return 0;
}
