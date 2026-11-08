#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libSceAudioOut/include/HostGain.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

void TestHostGainRampConvergence() {
    HostGainRamp defaultRamp;
    Require(defaultRamp.Current() == HostGainRamp::Unity);
    Require(defaultRamp.Step() == HostGainRamp::Unity / HostGainRamp::DefaultRampFrames);
    Require(defaultRamp.Bypass(HostGainRamp::Unity));

    HostGainRamp zeroRamp(0);
    Require(zeroRamp.Step() == HostGainRamp::Unity);

    HostGainRamp ramp(480);
    Require(ramp.Step() == 136);

    std::array<std::int16_t, 600> pcm16{};
    pcm16.fill(16000);

    ramp.Apply(pcm16.data(), 1, 1, 0);
    Require(ramp.Current() == HostGainRamp::Unity - 136);

    ramp.Apply(pcm16.data() + 1, 479, 1, 0);
    Require(ramp.Current() == 0);
    Require(pcm16[479] == 0);

    ramp.Apply(pcm16.data() + 480, 120, 1, 0);
    Require(ramp.Current() == 0);
    Require(pcm16[599] == 0);

    ramp.Apply(pcm16.data(), 480, 1, HostGainRamp::Unity);
    Require(ramp.Current() == HostGainRamp::Unity);
    Require(ramp.Bypass(HostGainRamp::Unity));

    HostGainRamp midRamp(480);
    std::array<std::int16_t, 300> midBuf{};
    midBuf.fill(10000);
    constexpr std::uint32_t halfGain = HostGainRamp::Unity / 2;
    midRamp.Apply(midBuf.data(), 240, 1, halfGain);
    Require(midRamp.Current() == halfGain);

    HostGainRamp floatRamp(200);
    Require(floatRamp.Step() == HostGainRamp::Unity / 200);
    std::array<float, 200> floatBuf{};
    floatBuf.fill(1.0f);
    floatRamp.Apply(floatBuf.data(), 200, 1, 0);
    Require(floatRamp.Current() == 0);
    Require(floatBuf[199] == 0.0f);

    HostGainRamp filterRamp(100);
    std::vector<std::uint8_t> scratch;
    std::array<float, 100> inData{};
    inData.fill(0.5f);
    const void* resPtr = HostGainFilter(filterRamp, 0, inData.data(), true, 100, 1, scratch);
    Require(resPtr != nullptr);
    Require(filterRamp.Current() == 0);
}

void TestMasterVolumeBounds() {
    Require(sceAudioOutInit() == 0 || sceAudioOutInit() == static_cast<int>(0x8026000e));

    const int port = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(port > 0);

    constexpr int volumeMax = 32768;
    std::array<int, 8> volMute = {0, 0, 0, 0, 0, 0, 0, 0};
    Require(sceAudioOutSetVolume(port, 3, volMute.data()) == 0);

    std::array<int, 8> volHalf = {16384, 16384, 0, 0, 0, 0, 0, 0};
    Require(sceAudioOutSetVolume(port, 3, volHalf.data()) == 0);

    std::array<int, 8> volMax = {volumeMax, volumeMax, 0, 0, 0, 0, 0, 0};
    Require(sceAudioOutSetVolume(port, 3, volMax.data()) == 0);

    Require(sceAudioOutSetVolume(port, 0, volMax.data()) == 0);
    Require(sceAudioOutSetVolume(port, 1, volMax.data()) == 0);
    Require(sceAudioOutSetVolume(port, 2, volMax.data()) == 0);
    Require(sceAudioOutSetVolume(port, 0xFF, volMax.data()) == 0);

    const int port8 = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 2);
    Require(port8 > 0);
    std::array<int, 8> vol8 = {32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768};
    Require(sceAudioOutSetVolume(port8, 0xFF, vol8.data()) == 0);
    Require(sceAudioOutClose(port8) == 0);

    const int portStd = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 6);
    Require(portStd > 0);
    Require(sceAudioOutSetVolume(portStd, 0xFF, vol8.data()) == 0);
    Require(sceAudioOutClose(portStd) == 0);

    Require(sceAudioOutClose(port) == 0);

    constexpr int invalidPort = static_cast<int>(0x80260001);
    constexpr int internalInvalidPort = static_cast<int>(0x80260003);
    const int closedVolRes = sceAudioOutSetVolume(port, 1, volMax.data());
    Require(closedVolRes == invalidPort || closedVolRes == internalInvalidPort || closedVolRes < 0);
}

void TestSampleFrequencyValidation() {
    constexpr int invalidParam = static_cast<int>(0x80260001);
    constexpr int invalidFreq = static_cast<int>(0x80260008);

    const int validPort = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(validPort > 0);
    Require(sceAudioOutClose(validPort) == 0);

    const int zeroFreqRes = sceAudioOutOpen(0x10000000, 0, 0, 256, 0, 1);
    Require(zeroFreqRes == invalidFreq || zeroFreqRes == invalidParam || zeroFreqRes < 0);

    const std::array<std::uint32_t, 3> otherFreqs = {44100, 96000, 192000};
    for (std::uint32_t freq : otherFreqs) {
        const int res = sceAudioOutOpen(0x10000000, 0, 0, 256, freq, 1);
        if (res > 0) {
            Require(sceAudioOutClose(res) == 0);
        } else {
            Require(res == invalidParam || res == invalidFreq || res < 0);
        }
    }
}

void TestInvalidPortHandleRejection() {
    constexpr int invalidPort = static_cast<int>(0x80260001);
    constexpr int internalInvalidPort = static_cast<int>(0x80260003);

    const std::array<int, 5> badHandles = {-1, 0, 33, 100, 999};
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

void TestNullPointerParameterRejection() {
    constexpr int invalidPointer = static_cast<int>(0x80260005);
    constexpr int internalInvalidPointer = static_cast<int>(0x80260004);

    const int port = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(port > 0);

    const int volNullRes = sceAudioOutSetVolume(port, 1, nullptr);
    Require(volNullRes == invalidPointer || volNullRes == internalInvalidPointer || volNullRes < 0);

    const int badPortVolNull = sceAudioOutSetVolume(-1, 1, nullptr);
    Require(badPortVolNull == invalidPointer || badPortVolNull == internalInvalidPointer || badPortVolNull < 0);

    const int stateNullRes = sceAudioOutGetPortState(port, nullptr);
    Require(stateNullRes == invalidPointer || stateNullRes == internalInvalidPointer || stateNullRes < 0);

    const int timeNullRes = sceAudioOutGetLastOutputTime(port, nullptr);
    Require(timeNullRes == invalidPointer || timeNullRes == internalInvalidPointer || timeNullRes < 0);

    const int outputsNullRes = sceAudioOutOutputs(nullptr, 1);
    Require(outputsNullRes == invalidPointer || outputsNullRes == internalInvalidPointer || outputsNullRes < 0);

    const int sysStateNullRes = sceAudioOutGetSystemState(nullptr);
    Require(sysStateNullRes == invalidPointer || sysStateNullRes == internalInvalidPointer || sysStateNullRes < 0);

    Require(sceAudioOutClose(port) == 0);
}

void TestMasteringVolumeLimits() {
    Require(sceAudioOutMasteringInit(0) == 0);

    AudioOut2MasteringParamsHeader params{1};
    Require(sceAudioOutMasteringSetParam(&params, 0) == 0);

    AudioOut2MasteringStates state{};
    state.states_header.states_id = 1;
    Require(sceAudioOutMasteringGetState(&state.states_header) == 0);
    Require(state.compressor_states.input_rms[0][0] == 0.0f);
    Require(state.limiter_states.input_peak[0] == 0.0f);

    Require(sceAudioOutMasteringTerm() == 0);
}

void TestPadSpeakerVolumeBounds() {
    const int padPort = sceAudioOutOpen(0x10000000, 4, 0, 256, 48000, 0);
    Require(padPort > 0);

    Require(sceAudioOutSetMixLevelPadSpk(padPort, 0) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 11626) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 32768) == 0);

    constexpr int invalidMixLevel = static_cast<int>(0x80260014);
    const int overMix = sceAudioOutSetMixLevelPadSpk(padPort, 32769);
    Require(overMix == invalidMixLevel || overMix < 0);

    std::array<int, 8> padVol = {32768, 0, 0, 0, 0, 0, 0, 0};
    Require(sceAudioOutSetVolume(padPort, 1, padVol.data()) == 0);

    Require(sceAudioOutClose(padPort) == 0);
}

} // namespace

int main() {
    TestHostGainRampConvergence();
    TestMasterVolumeBounds();
    TestSampleFrequencyValidation();
    TestInvalidPortHandleRejection();
    TestNullPointerParameterRejection();
    TestMasteringVolumeLimits();
    TestPadSpeakerVolumeBounds();

    LibcRunShutdown_nid_postfix();
    return 0;
}
