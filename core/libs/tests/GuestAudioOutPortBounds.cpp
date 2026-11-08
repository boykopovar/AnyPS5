#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libSceAudioOut/include/HostGain.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

template <typename F>
void RequireThrows(F f) {
    bool threw = false;
    try {
        f();
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw);
}

void TestPortManagement() {
    Require(sceAudioOutInit() == 0 || sceAudioOutInit() == static_cast<int>(0x8026000e));

    const int port = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(port > 0);
    Require(sceAudioOutClose(port) == 0);

    constexpr int invalidPort = static_cast<int>(0x80260001);
    constexpr int internalInvalidPort = static_cast<int>(0x80260003);
    const int closeAgain = sceAudioOutClose(port);
    Require(closeAgain == invalidPort || closeAgain == internalInvalidPort || closeAgain < 0);

    std::array<int, 32> ports{};
    for (std::size_t i = 0; i < ports.size(); ++i) {
        ports[i] = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
        Require(ports[i] > 0);
    }

    const int excessPort = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(excessPort == static_cast<int>(0x80260005) || excessPort == static_cast<int>(0x80260007) || excessPort < 0);

    Require(sceAudioOutClose(ports[0]) == 0);
    const int recycled = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(recycled > 0);
    Require(sceAudioOutClose(recycled) == 0);

    for (std::size_t i = 1; i < ports.size(); ++i) {
        Require(sceAudioOutClose(ports[i]) == 0);
    }
}

void TestInvalidPortRejection() {
    constexpr int invalidPort = static_cast<int>(0x80260001);
    constexpr int internalInvalidPort = static_cast<int>(0x80260003);

    const std::array<int, 4> badHandles = {-1, 0, 33, 999};
    std::array<int, 8> volume = {32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768};
    std::array<std::int16_t, 512> samples{};
    std::uint64_t outputTime = 0;
    AudioOutPortState state{};

    for (int handle : badHandles) {
        const int closeRes = sceAudioOutClose(handle);
        Require(closeRes == invalidPort || closeRes == internalInvalidPort || closeRes < 0);

        const int outRes = sceAudioOutOutput(handle, samples.data());
        Require(outRes == invalidPort || outRes == internalInvalidPort || outRes < 0);

        const int volRes = sceAudioOutSetVolume(handle, 1, volume.data());
        Require(volRes == invalidPort || volRes == internalInvalidPort || volRes < 0);

        const int timeRes = sceAudioOutGetLastOutputTime(handle, &outputTime);
        Require(timeRes == invalidPort || timeRes == internalInvalidPort || timeRes < 0);

        const int stateRes = sceAudioOutGetPortState(handle, &state);
        Require(stateRes == invalidPort || stateRes == internalInvalidPort || stateRes < 0);

        const int mixRes = sceAudioOutSetMixLevelPadSpk(handle, 11626);
        Require(mixRes == invalidPort || mixRes == internalInvalidPort || mixRes < 0);
    }
}

void TestInvalidPointerRejection() {
    constexpr int invalidPointer = static_cast<int>(0x80260005);
    constexpr int internalInvalidPointer = static_cast<int>(0x80260004);

    const int port = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(port > 0);

    const int volRes = sceAudioOutSetVolume(port, 1, nullptr);
    Require(volRes == invalidPointer || volRes == internalInvalidPointer || volRes < 0);

    const int timeRes = sceAudioOutGetLastOutputTime(port, nullptr);
    Require(timeRes == invalidPointer || timeRes == internalInvalidPointer || timeRes < 0);

    const int stateRes = sceAudioOutGetPortState(port, nullptr);
    Require(stateRes == invalidPointer || stateRes == internalInvalidPointer || stateRes < 0);

    const int outputsRes = sceAudioOutOutputs(nullptr, 1);
    Require(outputsRes == invalidPointer || outputsRes == internalInvalidPointer || outputsRes < 0);

    AudioOutOutputParam dummyParam{port, nullptr};
    const int zeroOutputsRes = sceAudioOutOutputs(&dummyParam, 0);
    Require(zeroOutputsRes == invalidPointer || zeroOutputsRes == internalInvalidPointer || zeroOutputsRes < 0);

    const int sysStateRes = sceAudioOutGetSystemState(nullptr);
    Require(sysStateRes == invalidPointer || sysStateRes == internalInvalidPointer || sysStateRes < 0);

    const int hdmiInfoRes = sceAudioOutSysGetHdmiMonitorInfo(1, nullptr, 0x180);
    Require(hdmiInfoRes == invalidPointer || hdmiInfoRes == internalInvalidPointer || hdmiInfoRes < 0);

    const int exInfoRes = sceAudioOutExGetMonitorInfo(0, 0, nullptr, 0x400);
    Require(exInfoRes == invalidPointer || exInfoRes == internalInvalidPointer || exInfoRes < 0);

    RequireThrows([] {
        sceAudioOutMasteringSetParam(nullptr, 0);
    });

    RequireThrows([] {
        sceAudioOutMasteringGetState(nullptr);
    });

    Require(sceAudioOutClose(port) == 0);
}

void TestSampleFrequencyValidation() {
    constexpr int invalidParam = static_cast<int>(0x80260001);
    constexpr int invalidSize = static_cast<int>(0x80260003);
    constexpr int internalInvalidFreq = static_cast<int>(0x80260008);
    constexpr int internalInvalidLen = static_cast<int>(0x80260006);

    const int zeroFreq = sceAudioOutOpen(0x10000000, 0, 0, 256, 0, 1);
    Require(zeroFreq == invalidParam || zeroFreq == internalInvalidFreq || zeroFreq < 0);

    const int zeroLen = sceAudioOutOpen(0x10000000, 0, 0, 0, 48000, 1);
    Require(zeroLen == invalidSize || zeroLen == internalInvalidLen || zeroLen < 0);

    const std::array<std::uint32_t, 4> standardGrainSizes = {256, 512, 1024, 2048};
    for (std::uint32_t grain : standardGrainSizes) {
        const int port = sceAudioOutOpen(0x10000000, 0, 0, grain, 48000, 1);
        Require(port > 0);
        Require(sceAudioOutClose(port) == 0);
    }

    const int activePort = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(activePort > 0);

    std::array<std::int16_t, 512> samples{};
    Require(sceAudioOutOutput(activePort, samples.data()) == 256);

    std::uint64_t lastTime = 0;
    Require(sceAudioOutGetLastOutputTime(activePort, &lastTime) == 0);
    Require(lastTime > 0);

    Require(sceAudioOutClose(activePort) == 0);
}

void TestChannelParameterBounds() {
    constexpr int invalidFormat = static_cast<int>(0x80260007);
    constexpr int invalidType = static_cast<int>(0x8026000A);
    constexpr int invalidParam = static_cast<int>(0x80260001);

    for (std::uint32_t fmt = 0; fmt <= 7; ++fmt) {
        const int port = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, fmt);
        Require(port > 0);
        Require(sceAudioOutClose(port) == 0);
    }

    const std::array<std::uint32_t, 4> badFormats = {8, 9, 15, 255};
    for (std::uint32_t badFmt : badFormats) {
        const int inv = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, badFmt);
        Require(inv == invalidFormat || inv < 0);
    }

    const std::array<int, 6> validTypes = {0, 1, 2, 3, 4, 127};
    for (int type : validTypes) {
        const int port = sceAudioOutOpen(0x10000000, type, 0, 256, 48000, 0);
        Require(port > 0);
        Require(sceAudioOutClose(port) == 0);
    }

    const std::array<int, 6> badTypes = {-1, 5, 8, 9, 20, 255};
    for (int badType : badTypes) {
        const int invType = sceAudioOutOpen(0x10000000, badType, 0, 256, 48000, 1);
        Require(invType == invalidType || invType == invalidParam || invType < 0);
    }

    const int mainPort = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(mainPort > 0);
    AudioOutPortState mainState{};
    Require(sceAudioOutGetPortState(mainPort, &mainState) == 0);
    Require(mainState.output == 1);
    Require(mainState.channel == 2);
    Require(mainState.volume == 127);

    Require(sceAudioOutSetMixLevelPadSpk(mainPort, 11626) == invalidType || sceAudioOutSetMixLevelPadSpk(mainPort, 11626) < 0);

    std::array<int, 8> volume = {32768, 32768, 0, 0, 0, 0, 0, 0};
    Require(sceAudioOutSetVolume(mainPort, 0, volume.data()) == 0);
    Require(sceAudioOutSetVolume(mainPort, 3, volume.data()) == 0);
    Require(sceAudioOutSetVolume(mainPort, 0xFF, volume.data()) == 0);
    Require(sceAudioOutClose(mainPort) == 0);

    const int voicePort = sceAudioOutOpen(0x10000000, 2, 0, 256, 48000, 0);
    Require(voicePort > 0);
    AudioOutPortState voiceState{};
    Require(sceAudioOutGetPortState(voicePort, &voiceState) == 0);
    Require(voiceState.output == 0x40);
    Require(voiceState.channel == 1);
    Require(sceAudioOutClose(voicePort) == 0);

    const int padPort = sceAudioOutOpen(0x10000000, 4, 0, 256, 48000, 0);
    Require(padPort > 0);
    AudioOutPortState padState{};
    Require(sceAudioOutGetPortState(padPort, &padState) == 0);
    Require(padState.output == 4);
    Require(padState.channel == 1);

    Require(sceAudioOutSetMixLevelPadSpk(padPort, 32768) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 0) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 11626) == 0);

    constexpr int invalidMixLevel = static_cast<int>(0x80260014);
    const int overMix = sceAudioOutSetMixLevelPadSpk(padPort, 32769);
    Require(overMix == invalidMixLevel || overMix < 0);

    Require(sceAudioOutClose(padPort) == 0);
}

void TestMasterVolumeRampParameters() {
    Require(sceAudioOutMasteringInit(0) == 0);

    AudioOut2MasteringParamsHeader params{1};
    Require(sceAudioOutMasteringSetParam(&params, 0) == 0);

    AudioOut2MasteringStates state{};
    state.states_header.states_id = 1;
    Require(sceAudioOutMasteringGetState(&state.states_header) == 0);
    Require(state.compressor_states.input_rms[0][0] == 0.0f);
    Require(state.limiter_states.input_peak[0] == 0.0f);

    HostGainRamp ramp(480);
    Require(ramp.Step() == 136);
    Require(ramp.Current() == HostGainRamp::Unity);
    Require(ramp.Bypass(HostGainRamp::Unity));

    std::array<std::int16_t, 600> audioData{};
    audioData.fill(10000);

    ramp.Apply(audioData.data(), 1, 1, 0);
    Require(ramp.Current() == HostGainRamp::Unity - 136);

    ramp.Apply(audioData.data() + 1, 599, 1, 0);
    Require(ramp.Current() == 0);
    Require(audioData.back() == 0);

    ramp.Apply(audioData.data(), 600, 1, HostGainRamp::Unity);
    Require(ramp.Current() == HostGainRamp::Unity);
    Require(ramp.Bypass(HostGainRamp::Unity));

    HostGainRamp fastRamp(1);
    Require(fastRamp.Step() == HostGainRamp::Unity);
    std::array<float, 4> floatData = {1.0f, 1.0f, 1.0f, 1.0f};
    fastRamp.Apply(floatData.data(), 4, 1, 0);
    Require(fastRamp.Current() == 0);
    Require(floatData[0] == 0.0f);

    Require(sceAudioOutMasteringTerm() == 0);
}

} // namespace

int main() {
    TestPortManagement();
    TestInvalidPortRejection();
    TestInvalidPointerRejection();
    TestSampleFrequencyValidation();
    TestChannelParameterBounds();
    TestMasterVolumeRampParameters();

    LibcRunShutdown_nid_postfix();
    return 0;
}
