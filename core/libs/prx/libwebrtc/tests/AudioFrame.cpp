#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <source_location>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc AudioFrame check failed at line %u\n", location.line());
    std::abort();
}

enum class Access { ReadWrite, ReadOnly, None };

struct GuardedPages {
    std::size_t pageSize = 0;
    std::size_t size = 0;
    unsigned char* base = nullptr;

    explicit GuardedPages(std::size_t bytes = 1) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        size = (bytes + pageSize - 1) / pageSize * pageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
#else
        pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        size = (bytes + pageSize - 1) / pageSize * pageSize;
        void* mapped = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        base = mapped == MAP_FAILED ? nullptr : static_cast<unsigned char*>(mapped);
#endif
        Require(base != nullptr);
    }

    ~GuardedPages() {
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, size);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    template<typename T> T* As(std::size_t offset = 0) const { return reinterpret_cast<T*>(base + offset); }

    void Protect(Access access, std::size_t offset = 0) const {
#ifdef _WIN32
        DWORD previous = 0;
        const DWORD mode = access == Access::ReadWrite ? PAGE_READWRITE : access == Access::ReadOnly ? PAGE_READONLY : PAGE_NOACCESS;
        Require(VirtualProtect(base + offset, size - offset, mode, &previous) != 0);
#else
        const int mode = access == Access::ReadWrite ? PROT_READ | PROT_WRITE : access == Access::ReadOnly ? PROT_READ : PROT_NONE;
        Require(mprotect(base + offset, size - offset, mode) == 0);
#endif
    }
};

template<typename TAction> bool ThrowsInvalidArgument(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
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
    auto frame = Filled();
    _ZN6webrtc10AudioFrameC1Ev_nid_postfix(source.get());
    _ZN6webrtc10AudioFrameC1Ev_nid_postfix(frame.get());
    WebRtcRtpPacketInfosData sourceInfos {};
    WebRtcRtpPacketInfosData frameInfos {};
    sourceInfos.refCount = 1;
    frameInfos.refCount = 2;
    source->packetInfos = &sourceInfos;
    frame->packetInfos = &frameInfos;
    source->timestamp = 7;
    source->elapsedTimeMs = 8;
    source->ntpTimeMs = 9;
    source->samplesPerChannel = 3;
    source->sampleRateHz = 16000;
    source->numChannels = 2;
    source->speechType = 1;
    source->vadActivity = 0;
    source->profileTimestampMs = 77;
    source->channelLayout = 3;
    source->absoluteCaptureTimestampMs.value = 1234;
    source->absoluteCaptureTimestampMsEngaged = true;
    source->muted = false;
    for (int i = 0; i < 6; ++i) source->data[i] = static_cast<std::int16_t>(100 + i);

    _ZN6webrtc10AudioFrame8CopyFromERKS0__nid_postfix(frame.get(), source.get());
    Require(frame->timestamp == 7 && frame->elapsedTimeMs == 8 && frame->ntpTimeMs == 9);
    Require(frame->samplesPerChannel == 3 && frame->sampleRateHz == 16000 && frame->numChannels == 2);
    Require(frame->speechType == 1 && frame->vadActivity == 0 && frame->channelLayout == 3);
    Require(frame->profileTimestampMs == 0);
    Require(frame->absoluteCaptureTimestampMsEngaged && frame->absoluteCaptureTimestampMs.value == 1234);
    Require(!frame->muted);
    Require(frame->packetInfos == &sourceInfos && sourceInfos.refCount == 2 && frameInfos.refCount == 1);
    for (int i = 0; i < 6; ++i) Require(frame->data[i] == 100 + i);
    for (std::size_t i = 6; i < WebRtcAudioFrameMaxDataSizeSamples; ++i) Require(frame->data[i] == 0);

    source->muted = true;
    source->packetInfos = nullptr;
    frame->data[0] = 55;
    _ZN6webrtc10AudioFrame8CopyFromERKS0__nid_postfix(frame.get(), source.get());
    Require(frame->muted && frame->data[0] == 55);
    Require(frame->packetInfos == nullptr && sourceInfos.refCount == 1);
    Require(AllSamples(_ZNK6webrtc10AudioFrame4dataEv_nid_postfix(frame.get()), 0));

    _ZN6webrtc10AudioFrame8CopyFromERKS0__nid_postfix(frame.get(), frame.get());
    Require(frame->muted && frame->data[0] == 55);

    source->muted = false;
    source->numChannels = 8;
    source->samplesPerChannel = 961;
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc10AudioFrame8CopyFromERKS0__nid_postfix(frame.get(), source.get()); });
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc10AudioFrame8CopyFromERKS0__nid_postfix(nullptr, source.get()); });
    RequireThrows<std::invalid_argument>([&] { _ZN6webrtc10AudioFrame8CopyFromERKS0__nid_postfix(frame.get(), nullptr); });
}

void TestMuteInaccessibleFrames() {
    GuardedPages hidden(sizeof(WebRtcAudioFrame));
    hidden.Protect(Access::None);
    Require(ThrowsInvalidArgument([&] { _ZN6webrtc20AudioFrameOperations4MuteEPNS_10AudioFrameE_nid_postfix(hidden.As<WebRtcAudioFrame>()); }));
    GuardedPages readOnly(sizeof(WebRtcAudioFrame));
    auto* frame = readOnly.As<WebRtcAudioFrame>();
    _ZN6webrtc10AudioFrameC1Ev_nid_postfix(frame);
    frame->muted = false;
    const auto before = std::make_unique<WebRtcAudioFrame>(*frame);
    readOnly.Protect(Access::ReadOnly);
    Require(ThrowsInvalidArgument([&] { _ZN6webrtc20AudioFrameOperations4MuteEPNS_10AudioFrameE_nid_postfix(frame); }));
    Require(std::memcmp(frame, before.get(), sizeof(WebRtcAudioFrame)) == 0);
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
    TestMuteInaccessibleFrames();
    return 0;
}
