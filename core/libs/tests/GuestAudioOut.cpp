#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

constexpr int AUDIO_OUT_ERROR_INVALID_PORT = static_cast<int>(0x80260003);
constexpr int audioOutErrorInvalidPort = AUDIO_OUT_ERROR_INVALID_PORT;
constexpr int invalidPort = AUDIO_OUT_ERROR_INVALID_PORT;
constexpr int invalidPointer = static_cast<int>(0x80260004);
constexpr int invalidSize = static_cast<int>(0x80260006);
constexpr int invalidFormat = static_cast<int>(0x80260007);
constexpr int invalidFreq = static_cast<int>(0x80260008);
constexpr int invalidPortType = static_cast<int>(0x8026000A);
constexpr int invalidMixLevel = static_cast<int>(0x80260014);
constexpr int alreadyInitialized = static_cast<int>(0x8026000E);

constexpr int user = 0x10000000;
constexpr int portTypeMain = 0;
constexpr int portTypeBgm = 1;
constexpr int portTypeVoice = 2;
constexpr int portTypePersonal = 3;
constexpr int portTypePadSpk = 4;
constexpr int portTypeVibration = 10;
constexpr int portTypeAudio3d = 126;
constexpr int portTypeAux = 127;

constexpr std::uint32_t standardFreq = 48000;
constexpr std::uint32_t standardLen = 256;
constexpr int unity = 32768;
constexpr int defaultMixLevel = 11626;

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

void TestPortHandleBounds() {
    constexpr int invalidHandles[] = {
        -1000,
        -1,
        0,
        33,
        100,
        1000,
        std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max()
    };

    AudioOutPortState state{};
    std::uint64_t lastTime = 0;
    int vol[8] = {unity, unity, unity, unity, unity, unity, unity, unity};

    for (const int handle : invalidHandles) {
        Require(sceAudioOutClose(handle) == AUDIO_OUT_ERROR_INVALID_PORT);
        Require(sceAudioOutOutput(handle, nullptr) == AUDIO_OUT_ERROR_INVALID_PORT);
        Require(sceAudioOutGetPortState(handle, &state) == AUDIO_OUT_ERROR_INVALID_PORT);
        Require(sceAudioOutGetLastOutputTime(handle, &lastTime) == AUDIO_OUT_ERROR_INVALID_PORT);
        Require(sceAudioOutSetVolume(handle, 0xFF, vol) == AUDIO_OUT_ERROR_INVALID_PORT);
        Require(sceAudioOutSetMixLevelPadSpk(handle, defaultMixLevel) == AUDIO_OUT_ERROR_INVALID_PORT);

        AudioOutOutputParam badParam{handle, nullptr};
        Require(sceAudioOutOutputs(&badParam, 1) == AUDIO_OUT_ERROR_INVALID_PORT);

        Require(sceAudioOutGetPortState(handle, nullptr) == invalidPointer);
        Require(sceAudioOutGetLastOutputTime(handle, nullptr) == invalidPointer);
        Require(sceAudioOutSetVolume(handle, 0xFF, nullptr) == invalidPointer);
    }

    AudioOutOutputParam validParam{1, nullptr};
    Require(sceAudioOutOutputs(nullptr, 1) == invalidPointer);
    Require(sceAudioOutOutputs(&validParam, 0) == invalidPointer);
}

void TestClosedPortHandleBounds() {
    const int handle = sceAudioOutOpen(user, portTypeMain, 0, standardLen, standardFreq, 1);
    Require(handle >= 1 && handle <= 32);

    Require(sceAudioOutClose(handle) == 0);
    Require(sceAudioOutClose(handle) == AUDIO_OUT_ERROR_INVALID_PORT);

    AudioOutPortState state{};
    std::uint64_t lastTime = 0;
    int vol[8] = {unity, unity, unity, unity, unity, unity, unity, unity};

    Require(sceAudioOutOutput(handle, nullptr) == AUDIO_OUT_ERROR_INVALID_PORT);
    Require(sceAudioOutGetPortState(handle, &state) == AUDIO_OUT_ERROR_INVALID_PORT);
    Require(sceAudioOutGetLastOutputTime(handle, &lastTime) == AUDIO_OUT_ERROR_INVALID_PORT);
    Require(sceAudioOutSetVolume(handle, 0xFF, vol) == AUDIO_OUT_ERROR_INVALID_PORT);
    Require(sceAudioOutSetMixLevelPadSpk(handle, defaultMixLevel) == AUDIO_OUT_ERROR_INVALID_PORT);

    AudioOutOutputParam badParam{handle, nullptr};
    Require(sceAudioOutOutputs(&badParam, 1) == AUDIO_OUT_ERROR_INVALID_PORT);
}

void TestSampleFrequencyValidation() {
    Require(sceAudioOutOpen(user, portTypeMain, 0, standardLen, 0, 1) == invalidFreq);
    Require(sceAudioOutOpen(user, portTypeMain, 0, 0, standardFreq, 1) == invalidSize);

    Require(sceAudioOutOpen(user, -1, 0, standardLen, standardFreq, 1) == invalidPortType);
    Require(sceAudioOutOpen(user, 5, 0, standardLen, standardFreq, 1) == invalidPortType);
    Require(sceAudioOutOpen(user, 11, 0, standardLen, standardFreq, 1) == invalidPortType);

    Require(sceAudioOutOpen(user, portTypeMain, 0, standardLen, standardFreq, 8) == invalidFormat);
    Require(sceAudioOutOpen(user, portTypeMain, 0, standardLen, standardFreq, 0xFF) == invalidFormat);
}

void TestVolumeBoundaries() {
    const int handle = sceAudioOutOpen(user, portTypeMain, 0, standardLen, standardFreq, 1);
    Require(handle >= 1 && handle <= 32);

    int vol[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    Require(sceAudioOutSetVolume(handle, 0, vol) == 0);
    Require(sceAudioOutSetVolume(handle, 0x1, vol) == 0);
    Require(sceAudioOutSetVolume(handle, 0x2, vol) == 0);
    Require(sceAudioOutSetVolume(handle, 0x3, vol) == 0);
    Require(sceAudioOutSetVolume(handle, 0xFF, vol) == 0);
    Require(sceAudioOutSetVolume(handle, 0xFFFFFFFFu, vol) == 0);

    vol[0] = unity;
    vol[1] = unity / 2;
    Require(sceAudioOutSetVolume(handle, 0x3, vol) == 0);

    vol[0] = -unity;
    vol[1] = -1;
    Require(sceAudioOutSetVolume(handle, 0x3, vol) == 0);

    vol[0] = std::numeric_limits<int>::max();
    vol[1] = std::numeric_limits<int>::min();
    Require(sceAudioOutSetVolume(handle, 0x3, vol) == 0);

    Require(sceAudioOutSetVolume(handle, 0x1, nullptr) == invalidPointer);
    Require(sceAudioOutClose(handle) == 0);

    const int padHandle = sceAudioOutOpen(user, portTypePadSpk, 0, standardLen, standardFreq, 0);
    Require(padHandle >= 1 && padHandle <= 32);

    Require(sceAudioOutSetMixLevelPadSpk(padHandle, 0) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padHandle, defaultMixLevel) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padHandle, unity) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padHandle, unity + 1) == invalidMixLevel);
    Require(sceAudioOutSetMixLevelPadSpk(padHandle, 0xFFFFFFFFu) == invalidMixLevel);
    Require(sceAudioOutClose(padHandle) == 0);

    const int otherHandle = sceAudioOutOpen(user, portTypeMain, 0, standardLen, standardFreq, 0);
    Require(otherHandle >= 1 && otherHandle <= 32);
    Require(sceAudioOutSetMixLevelPadSpk(otherHandle, defaultMixLevel) == invalidPortType);
    Require(sceAudioOutSetMixLevelPadSpk(otherHandle, unity + 1) == invalidMixLevel);
    Require(sceAudioOutClose(otherHandle) == 0);
}

void TestMillibelVolumeBitmaskClamping() {
    const int handle = sceAudioOutOpen(user, portTypeMain, 0, standardLen, standardFreq, 2);
    Require(handle >= 1 && handle <= 32);

    constexpr std::uint32_t bitmasks[] = {
        0x00u,
        0x01u,
        0x02u,
        0x04u,
        0x08u,
        0x10u,
        0x20u,
        0x40u,
        0x80u,
        0x03u,
        0x0Fu,
        0x3Fu,
        0x55u,
        0xAAu,
        0xFFu,
        0xFFFFFFFFu
    };

    int nominalVol[8] = {-10000, -5000, -3000, -1200, -600, -300, -100, 0};
    for (const auto mask : bitmasks) {
        Require(sceAudioOutSetVolume(handle, mask, nominalVol) == 0);
    }

    int boundaryVol[8] = {-10000, -10000, -5000, -5000, 0, 0, -10000, 0};
    Require(sceAudioOutSetVolume(handle, 0xFF, boundaryVol) == 0);

    int clampedVol[8] = {-50000, -20000, -10001, -10000, 0, 1, 1000, 10000};
    Require(sceAudioOutSetVolume(handle, 0xFF, clampedVol) == 0);

    Require(sceAudioOutSetVolume(handle, 0xFF, nullptr) == invalidPointer);
    Require(sceAudioOutSetVolume(0, 0xFF, nominalVol) == AUDIO_OUT_ERROR_INVALID_PORT);
    Require(sceAudioOutSetVolume(-1, 0xFF, nominalVol) == AUDIO_OUT_ERROR_INVALID_PORT);
    Require(sceAudioOutSetVolume(-1000, 0xFF, nominalVol) == AUDIO_OUT_ERROR_INVALID_PORT);
    Require(sceAudioOutSetVolume(33, 0xFF, nominalVol) == AUDIO_OUT_ERROR_INVALID_PORT);
    Require(sceAudioOutSetVolume(100, 0xFF, nominalVol) == AUDIO_OUT_ERROR_INVALID_PORT);
    Require(sceAudioOutSetVolume(std::numeric_limits<int>::min(), 0xFF, nominalVol) == AUDIO_OUT_ERROR_INVALID_PORT);
    Require(sceAudioOutSetVolume(std::numeric_limits<int>::max(), 0xFF, nominalVol) == AUDIO_OUT_ERROR_INVALID_PORT);

    Require(sceAudioOutClose(handle) == 0);
    Require(sceAudioOutSetVolume(handle, 0xFF, nominalVol) == AUDIO_OUT_ERROR_INVALID_PORT);
}

void TestSurround71DownmixingValidation() {
    constexpr float fold = 0.70710678f;
    constexpr std::uint32_t surroundFormats[] = {2u, 5u, 6u, 7u};
    constexpr std::uint32_t grainSizes[] = {256u, 512u, 1024u};

    for (const auto format : surroundFormats) {
        for (const auto grain : grainSizes) {
            const int handle = sceAudioOutOpen(user, portTypeMain, 0, grain, standardFreq, format);
            Require(handle >= 1 && handle <= 32);

            AudioOutPortState state{};
            std::uint64_t lastTime = 0;
            Require(sceAudioOutGetPortState(handle, &state) == 0);
            Require(state.output == 1);
            Require(state.channel == 2);

            const bool isFloat = (format == 5u || format == 7u);
            const std::size_t sampleBytes = isFloat ? sizeof(float) : sizeof(std::int16_t);
            const std::size_t totalBytes = static_cast<std::size_t>(grain) * 8u * sampleBytes;
            std::vector<std::uint8_t> buffer(totalBytes, 0);

            if (isFloat) {
                auto* samples = reinterpret_cast<float*>(buffer.data());
                for (std::uint32_t fr = 0; fr < grain; ++fr) {
                    samples[fr * 8 + 0] = 1.0f;
                    samples[fr * 8 + 1] = 1.0f;
                    samples[fr * 8 + 2] = fold;
                    samples[fr * 8 + 3] = 0.0f;
                    samples[fr * 8 + 4] = fold;
                    samples[fr * 8 + 5] = fold;
                    samples[fr * 8 + 6] = fold;
                    samples[fr * 8 + 7] = fold;
                }
            } else {
                auto* samples = reinterpret_cast<std::int16_t*>(buffer.data());
                for (std::uint32_t fr = 0; fr < grain; ++fr) {
                    samples[fr * 8 + 0] = 16384;
                    samples[fr * 8 + 1] = 16384;
                    samples[fr * 8 + 2] = static_cast<std::int16_t>(16384 * fold);
                    samples[fr * 8 + 3] = 0;
                    samples[fr * 8 + 4] = static_cast<std::int16_t>(16384 * fold);
                    samples[fr * 8 + 5] = static_cast<std::int16_t>(16384 * fold);
                    samples[fr * 8 + 6] = static_cast<std::int16_t>(16384 * fold);
                    samples[fr * 8 + 7] = static_cast<std::int16_t>(16384 * fold);
                }
            }

            Require(sceAudioOutOutput(handle, buffer.data()) == static_cast<int>(grain));
            Require(sceAudioOutOutput(handle, nullptr) == static_cast<int>(grain));

            Require(sceAudioOutClose(handle) == 0);
            Require(sceAudioOutClose(handle) == AUDIO_OUT_ERROR_INVALID_PORT);
            Require(sceAudioOutOutput(handle, nullptr) == AUDIO_OUT_ERROR_INVALID_PORT);
            Require(sceAudioOutGetPortState(handle, &state) == AUDIO_OUT_ERROR_INVALID_PORT);
            Require(sceAudioOutGetLastOutputTime(handle, &lastTime) == AUDIO_OUT_ERROR_INVALID_PORT);
        }

        bool vibrationThrew = false;
        try {
            static_cast<void>(sceAudioOutOpen(user, portTypeVibration, 0, standardLen, standardFreq, format));
        } catch (const std::runtime_error&) {
            vibrationThrew = true;
        }
        Require(vibrationThrew);
    }
}

void TestChannelConfigurations() {
    struct FormatConfig {
        std::uint32_t formatParam;
        std::uint32_t channelCount;
        bool isFloat;
        std::uint8_t expectedReportedChannels;
    };

    constexpr FormatConfig configs[] = {
        {0, 1, false, 1},
        {1, 2, false, 2},
        {2, 8, false, 2},
        {3, 1, true, 1},
        {4, 2, true, 2},
        {5, 8, true, 2},
        {6, 8, false, 2},
        {7, 8, true, 2}
    };

    for (const auto& config : configs) {
        const int handle = sceAudioOutOpen(user, portTypeMain, 0, standardLen, standardFreq, config.formatParam);
        Require(handle >= 1 && handle <= 32);

        AudioOutPortState state{};
        Require(sceAudioOutGetPortState(handle, &state) == 0);
        Require(state.output == 1);
        Require(state.channel == config.expectedReportedChannels);

        int vol[8] = {unity, unity, unity, unity, unity, unity, unity, unity};
        Require(sceAudioOutSetVolume(handle, 0xFF, vol) == 0);

        const std::size_t sampleBytes = config.isFloat ? sizeof(float) : sizeof(std::int16_t);
        const std::size_t totalBytes = static_cast<std::size_t>(standardLen) * config.channelCount * sampleBytes;
        std::vector<std::uint8_t> buffer(totalBytes, 0);

        Require(sceAudioOutOutput(handle, buffer.data()) == static_cast<int>(standardLen));
        Require(sceAudioOutOutput(handle, nullptr) == static_cast<int>(standardLen));

        Require(sceAudioOutClose(handle) == 0);
    }

    const int vibMono = sceAudioOutOpen(user, portTypeVibration, 0, standardLen, standardFreq, 0);
    Require(vibMono >= 1 && vibMono <= 32);
    Require(sceAudioOutClose(vibMono) == 0);

    const int vibStereo = sceAudioOutOpen(user, portTypeVibration, 0, standardLen, standardFreq, 1);
    Require(vibStereo >= 1 && vibStereo <= 32);
    Require(sceAudioOutClose(vibStereo) == 0);

    bool vibrationRejected = false;
    try {
        static_cast<void>(sceAudioOutOpen(user, portTypeVibration, 0, standardLen, standardFreq, 2));
    } catch (const std::runtime_error&) {
        vibrationRejected = true;
    }
    Require(vibrationRejected);
}

void TestMultiChannelBatchOutputs() {
    const int monoPort = sceAudioOutOpen(user, portTypeMain, 0, standardLen, standardFreq, 0);
    const int stereoPort = sceAudioOutOpen(user, portTypeMain, 0, standardLen, standardFreq, 1);
    const int eightChPort = sceAudioOutOpen(user, portTypeMain, 0, standardLen, standardFreq, 2);

    Require(monoPort >= 1 && monoPort <= 32);
    Require(stereoPort >= 1 && stereoPort <= 32);
    Require(eightChPort >= 1 && eightChPort <= 32);

    std::vector<std::int16_t> monoBuf(standardLen, 0);
    std::vector<std::int16_t> stereoBuf(static_cast<std::size_t>(standardLen) * 2, 0);
    std::vector<std::int16_t> eightChBuf(static_cast<std::size_t>(standardLen) * 8, 0);

    AudioOutOutputParam batch[3] = {
        {monoPort, monoBuf.data()},
        {stereoPort, stereoBuf.data()},
        {eightChPort, eightChBuf.data()}
    };

    Require(sceAudioOutOutputs(batch, 3) == static_cast<int>(standardLen));

    Require(sceAudioOutClose(monoPort) == 0);
    Require(sceAudioOutClose(stereoPort) == 0);
    Require(sceAudioOutClose(eightChPort) == 0);
}

void TestStandardFrequency48000HzPortLifecycle() {
    constexpr int portTypes[] = {
        portTypeMain,
        portTypeBgm,
        portTypeVoice,
        portTypePersonal,
        portTypePadSpk,
        portTypeAux,
        portTypeAudio3d
    };

    for (const int type : portTypes) {
        const int handle = sceAudioOutOpen(user, type, 0, standardLen, standardFreq, 0);
        Require(handle >= 1 && handle <= 32);

        AudioOutPortState state{};
        Require(sceAudioOutGetPortState(handle, &state) == 0);
        Require(state.volume == 127);

        std::uint64_t lastTime = 0;
        Require(sceAudioOutGetLastOutputTime(handle, &lastTime) == 0);

        int vol[8] = {unity, unity, unity, unity, unity, unity, unity, unity};
        Require(sceAudioOutSetVolume(handle, 0x1, vol) == 0);

        Require(sceAudioOutOutput(handle, nullptr) == static_cast<int>(standardLen));

        if (type == portTypePadSpk) {
            Require(sceAudioOutSetMixLevelPadSpk(handle, defaultMixLevel) == 0);
            Require(sceAudioOutSetMixLevelPadSpk(handle, 40000) == invalidMixLevel);
        } else {
            Require(sceAudioOutSetMixLevelPadSpk(handle, defaultMixLevel) == invalidPortType);
        }

        AudioOutOutputParam batchParam{handle, nullptr};
        Require(sceAudioOutOutputs(&batchParam, 1) == static_cast<int>(standardLen));

        Require(sceAudioOutClose(handle) == 0);
        Require(sceAudioOutClose(handle) == AUDIO_OUT_ERROR_INVALID_PORT);
        Require(sceAudioOutOutput(handle, nullptr) == AUDIO_OUT_ERROR_INVALID_PORT);
        Require(sceAudioOutGetPortState(handle, &state) == AUDIO_OUT_ERROR_INVALID_PORT);
        Require(sceAudioOutGetLastOutputTime(handle, &lastTime) == AUDIO_OUT_ERROR_INVALID_PORT);
    }
}

void TestSystemState() {
    AudioOutSystemState sysState{};
    Require(sceAudioOutGetSystemState(&sysState) == 0);
    Require(sceAudioOutGetSystemState(nullptr) == invalidPointer);
}

void TestPortStateAudit() {
    struct PortTypeDescriptor {
        int type;
        std::uint16_t expectedOutput;
        std::uint8_t expectedMonoChannels;
        std::uint8_t expectedStereoChannels;
        bool supportsStereo;
    };

    constexpr PortTypeDescriptor descriptors[] = {
        {portTypeMain, 1, 1, 2, true},
        {portTypeBgm, 1, 1, 2, true},
        {portTypeVoice, 0x40, 1, 1, false},
        {portTypePersonal, 0x40, 1, 1, false},
        {portTypePadSpk, 4, 1, 1, false},
        {portTypeAudio3d, 1, 1, 2, true},
        {portTypeAux, 0x80, 0, 0, false}
    };

    for (const auto& desc : descriptors) {
        const int monoHandle = sceAudioOutOpen(user, desc.type, 0, standardLen, standardFreq, 0);
        Require(monoHandle >= 1 && monoHandle <= 32);

        AudioOutPortState state{};
        Require(sceAudioOutGetPortState(monoHandle, &state) == 0);
        Require(state.output == desc.expectedOutput);
        Require(state.channel == desc.expectedMonoChannels);
        if (desc.type != portTypeAux) {
            Require(state.channel > 0);
        }
        Require(state.volume == 127);
        Require(state.rerouteCounter == 0);
        Require(state.flag == 0);
        Require(state.activeState == 0);
        Require(state.reserved[0] == 0);

        constexpr std::uint32_t bitmasks[] = {0x00u, 0x01u, 0x02u, 0x03u, 0x0Fu, 0xFFu, 0xFFFFFFFFu};
        int vol[8] = {-1000, -2000, -3000, -4000, 0, 0, 0, 0};
        for (const auto mask : bitmasks) {
            Require(sceAudioOutSetVolume(monoHandle, mask, vol) == 0);
            Require(sceAudioOutGetPortState(monoHandle, &state) == 0);
            Require(state.output == desc.expectedOutput);
            Require(state.channel == desc.expectedMonoChannels);
            Require(state.volume == 127);
            Require(state.flag == 0);
            Require(state.activeState == 0);
        }

        Require(sceAudioOutClose(monoHandle) == 0);
        Require(sceAudioOutGetPortState(monoHandle, &state) == AUDIO_OUT_ERROR_INVALID_PORT);

        if (desc.supportsStereo) {
            const int stereoHandle = sceAudioOutOpen(user, desc.type, 0, standardLen, standardFreq, 1);
            Require(stereoHandle >= 1 && stereoHandle <= 32);

            AudioOutPortState stereoState{};
            Require(sceAudioOutGetPortState(stereoHandle, &stereoState) == 0);
            Require(stereoState.output == desc.expectedOutput);
            Require(stereoState.channel == desc.expectedStereoChannels);
            Require(stereoState.channel > 0);
            Require(stereoState.volume == 127);

            Require(sceAudioOutClose(stereoHandle) == 0);
            Require(sceAudioOutGetPortState(stereoHandle, &stereoState) == AUDIO_OUT_ERROR_INVALID_PORT);
        }
    }

    const int vibHandle = sceAudioOutOpen(user, portTypeVibration, 0, standardLen, standardFreq, 0);
    Require(vibHandle >= 1 && vibHandle <= 32);
    AudioOutPortState vibState{};
    Require(sceAudioOutGetPortState(vibHandle, &vibState) == 0);
    Require(vibState.output == 4);
    Require(vibState.channel == 1);
    Require(vibState.channel > 0);
    Require(vibState.volume == 127);
    Require(sceAudioOutClose(vibHandle) == 0);
    Require(sceAudioOutGetPortState(vibHandle, &vibState) == AUDIO_OUT_ERROR_INVALID_PORT);
}

void TestPadSpeakerMixLevelAndRouting() {
    const int padHandle = sceAudioOutOpen(user, portTypePadSpk, 0, standardLen, standardFreq, 0);
    Require(padHandle >= 1 && padHandle <= 32);

    constexpr std::uint32_t validMixLevels[] = {
        0u,
        1u,
        64u,
        127u,
        128u,
        255u,
        500u,
        1000u,
        static_cast<std::uint32_t>(defaultMixLevel),
        16384u,
        static_cast<std::uint32_t>(unity)
    };

    for (const auto level : validMixLevels) {
        Require(sceAudioOutSetMixLevelPadSpk(padHandle, level) == 0);
    }

    constexpr std::uint32_t outOfBoundLevels[] = {
        static_cast<std::uint32_t>(unity + 1),
        32769u,
        35000u,
        40000u,
        65535u,
        65536u,
        100000u,
        0xFFFFFFFFu
    };

    for (const auto level : outOfBoundLevels) {
        Require(sceAudioOutSetMixLevelPadSpk(padHandle, level) == invalidMixLevel);
    }

    Require(sceAudioOutClose(padHandle) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padHandle, defaultMixLevel) == AUDIO_OUT_ERROR_INVALID_PORT);
    Require(sceAudioOutSetMixLevelPadSpk(padHandle, 0) == AUDIO_OUT_ERROR_INVALID_PORT);
    Require(sceAudioOutSetMixLevelPadSpk(padHandle, 255) == AUDIO_OUT_ERROR_INVALID_PORT);
    Require(sceAudioOutSetMixLevelPadSpk(padHandle, unity + 1) == AUDIO_OUT_ERROR_INVALID_PORT);

    constexpr int nonPadPortTypes[] = {
        portTypeMain,
        portTypeBgm,
        portTypeVoice,
        portTypePersonal,
        portTypeAux,
        portTypeAudio3d
    };

    for (const int type : nonPadPortTypes) {
        const int handle = sceAudioOutOpen(user, type, 0, standardLen, standardFreq, 0);
        Require(handle >= 1 && handle <= 32);

        for (const auto level : validMixLevels) {
            Require(sceAudioOutSetMixLevelPadSpk(handle, level) == invalidPortType);
        }

        for (const auto level : outOfBoundLevels) {
            Require(sceAudioOutSetMixLevelPadSpk(handle, level) == invalidMixLevel);
        }

        Require(sceAudioOutClose(handle) == 0);
        Require(sceAudioOutSetMixLevelPadSpk(handle, defaultMixLevel) == AUDIO_OUT_ERROR_INVALID_PORT);
    }

    const int vibHandle = sceAudioOutOpen(user, portTypeVibration, 0, standardLen, standardFreq, 0);
    Require(vibHandle >= 1 && vibHandle <= 32);
    Require(sceAudioOutSetMixLevelPadSpk(vibHandle, defaultMixLevel) == invalidPortType);
    Require(sceAudioOutSetMixLevelPadSpk(vibHandle, unity + 1) == invalidMixLevel);
    Require(sceAudioOutClose(vibHandle) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(vibHandle, defaultMixLevel) == AUDIO_OUT_ERROR_INVALID_PORT);

    constexpr int invalidHandles[] = {
        -1000,
        -1,
        0,
        33,
        100,
        std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max()
    };

    for (const int handle : invalidHandles) {
        Require(sceAudioOutSetMixLevelPadSpk(handle, 0) == AUDIO_OUT_ERROR_INVALID_PORT);
        Require(sceAudioOutSetMixLevelPadSpk(handle, 127) == AUDIO_OUT_ERROR_INVALID_PORT);
        Require(sceAudioOutSetMixLevelPadSpk(handle, 255) == AUDIO_OUT_ERROR_INVALID_PORT);
        Require(sceAudioOutSetMixLevelPadSpk(handle, defaultMixLevel) == AUDIO_OUT_ERROR_INVALID_PORT);
        Require(sceAudioOutSetMixLevelPadSpk(handle, unity) == AUDIO_OUT_ERROR_INVALID_PORT);
        Require(sceAudioOutSetMixLevelPadSpk(handle, unity + 1) == AUDIO_OUT_ERROR_INVALID_PORT);
    }
}

}

int main() {
#ifdef _WIN32
    _putenv_s("SDL_AUDIODRIVER", "dummy");
#else
    setenv("SDL_AUDIODRIVER", "dummy", 1);
#endif

    Require(sceAudioOutInit() == 0);
    Require(sceAudioOutInit() == alreadyInitialized);

    TestPortHandleBounds();
    TestClosedPortHandleBounds();
    TestSampleFrequencyValidation();
    TestVolumeBoundaries();
    TestMillibelVolumeBitmaskClamping();
    TestSurround71DownmixingValidation();
    TestChannelConfigurations();
    TestMultiChannelBatchOutputs();
    TestStandardFrequency48000HzPortLifecycle();
    TestSystemState();
    TestPortStateAudit();
    TestPadSpeakerMixLevelAndRouting();

    LibcRunShutdown_nid_postfix();
    return 0;
}
