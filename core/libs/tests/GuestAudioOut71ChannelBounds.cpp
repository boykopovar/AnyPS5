#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace {

constexpr int testUser = 0x10000000;
constexpr int errorInvalidPortType = -2144993270;
constexpr int errorInvalidPort = -2144993277;
constexpr int errorInvalidPointer = -2144993276;
constexpr int errorPortFull = -2144993275;
constexpr int errorInvalidSize = -2144993274;
constexpr int errorInvalidFormat = static_cast<int>(0x80260007);
constexpr int errorInvalidFreq = -2144993272;
constexpr int errorAlreadyInitialized = static_cast<int>(0x8026000e);

constexpr std::uint32_t formatS16_8Ch = 2;
constexpr std::uint32_t formatF32_8Ch = 5;
constexpr std::uint32_t formatS16_8ChStd = 6;
constexpr std::uint32_t formatF32_8ChStd = 7;

constexpr std::array<std::uint32_t, 4> channel71Formats = {
    formatS16_8Ch,
    formatF32_8Ch,
    formatS16_8ChStd,
    formatF32_8ChStd
};

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestAudioOutInit() {
    const int rc = sceAudioOutInit();
    Require(rc == 0 || rc == errorAlreadyInitialized);
}

void Test71ChannelOpenParameterBounds() {
    for (const auto fmt : channel71Formats) {
        const int h = sceAudioOutOpen(testUser, 0, 0, 256, 48000, fmt);
        Require(h >= 1 && h <= 32);
        Require(sceAudioOutClose(h) == 0);

        Require(sceAudioOutOpen(testUser, 0, 0, 0, 48000, fmt) == errorInvalidSize);
        Require(sceAudioOutOpen(testUser, 0, 0, 256, 0, fmt) == errorInvalidFreq);

        Require(sceAudioOutOpen(testUser, -1, 0, 256, 48000, fmt) == errorInvalidPortType);
        Require(sceAudioOutOpen(testUser, 5, 0, 256, 48000, fmt) == errorInvalidPortType);
        Require(sceAudioOutOpen(testUser, 8, 0, 256, 48000, fmt) == errorInvalidPortType);
        Require(sceAudioOutOpen(testUser, 9, 0, 256, 48000, fmt) == errorInvalidPortType);
        Require(sceAudioOutOpen(testUser, 11, 0, 256, 48000, fmt) == errorInvalidPortType);
        Require(sceAudioOutOpen(testUser, 125, 0, 256, 48000, fmt) == errorInvalidPortType);
        Require(sceAudioOutOpen(testUser, 128, 0, 256, 48000, fmt) == errorInvalidPortType);

        bool vibrationThrew = false;
        try {
            sceAudioOutOpen(testUser, 10, 0, 256, 48000, fmt);
        } catch (const std::runtime_error&) {
            vibrationThrew = true;
        }
        Require(vibrationThrew);

        const int hWithUpperBits = sceAudioOutOpen(testUser, 0, 0, 512, 48000, 0xAA00u | fmt);
        Require(hWithUpperBits >= 1 && hWithUpperBits <= 32);
        Require(sceAudioOutClose(hWithUpperBits) == 0);
    }

    Require(sceAudioOutOpen(testUser, 0, 0, 256, 48000, 8) == errorInvalidFormat);
    Require(sceAudioOutOpen(testUser, 0, 0, 256, 48000, 9) == errorInvalidFormat);
    Require(sceAudioOutOpen(testUser, 0, 0, 256, 48000, 0x10) == errorInvalidFormat);
    Require(sceAudioOutOpen(testUser, 0, 0, 256, 48000, 0x7F) == errorInvalidFormat);
    Require(sceAudioOutOpen(testUser, 0, 0, 256, 48000, 0xFF) == errorInvalidFormat);
}

void Test71ChannelPortStateBounds() {
    AudioOutPortState state{};

    Require(sceAudioOutGetPortState(0, nullptr) == errorInvalidPointer);
    Require(sceAudioOutGetPortState(-1, nullptr) == errorInvalidPointer);
    Require(sceAudioOutGetPortState(33, nullptr) == errorInvalidPointer);
    Require(sceAudioOutGetPortState(999, nullptr) == errorInvalidPointer);

    Require(sceAudioOutGetPortState(0, &state) == errorInvalidPort);
    Require(sceAudioOutGetPortState(-1, &state) == errorInvalidPort);
    Require(sceAudioOutGetPortState(33, &state) == errorInvalidPort);

    for (const auto fmt : channel71Formats) {
        const int hMain = sceAudioOutOpen(testUser, 0, 0, 256, 48000, fmt);
        Require(hMain > 0);
        Require(sceAudioOutGetPortState(hMain, nullptr) == errorInvalidPointer);
        state = {};
        Require(sceAudioOutGetPortState(hMain, &state) == 0);
        Require(state.output == 1);
        Require(state.channel == 2);
        Require(state.volume == 127);
        Require(state.rerouteCounter == 0);
        Require(state.flag == 0);
        Require(state.activeState == 0);
        Require(sceAudioOutClose(hMain) == 0);

        const int hBgm = sceAudioOutOpen(testUser, 1, 0, 256, 48000, fmt);
        Require(hBgm > 0);
        state = {};
        Require(sceAudioOutGetPortState(hBgm, &state) == 0);
        Require(state.output == 1);
        Require(state.channel == 2);
        Require(sceAudioOutClose(hBgm) == 0);

        const int hVoice = sceAudioOutOpen(testUser, 2, 0, 256, 48000, fmt);
        Require(hVoice > 0);
        state = {};
        Require(sceAudioOutGetPortState(hVoice, &state) == 0);
        Require(state.output == 0x40);
        Require(state.channel == 1);
        Require(sceAudioOutClose(hVoice) == 0);

        const int hPersonal = sceAudioOutOpen(testUser, 3, 0, 256, 48000, fmt);
        Require(hPersonal > 0);
        state = {};
        Require(sceAudioOutGetPortState(hPersonal, &state) == 0);
        Require(state.output == 0x40);
        Require(state.channel == 1);
        Require(sceAudioOutClose(hPersonal) == 0);

        const int hPadSpk = sceAudioOutOpen(testUser, 4, 0, 256, 48000, fmt);
        Require(hPadSpk > 0);
        state = {};
        Require(sceAudioOutGetPortState(hPadSpk, &state) == 0);
        Require(state.output == 4);
        Require(state.channel == 1);
        Require(sceAudioOutClose(hPadSpk) == 0);

        const int hAudio3D = sceAudioOutOpen(testUser, 126, 0, 256, 48000, fmt);
        Require(hAudio3D > 0);
        state = {};
        Require(sceAudioOutGetPortState(hAudio3D, &state) == 0);
        Require(state.output == 1);
        Require(state.channel == 2);
        Require(sceAudioOutClose(hAudio3D) == 0);

        const int hAux = sceAudioOutOpen(testUser, 127, 0, 256, 48000, fmt);
        Require(hAux > 0);
        state = {};
        Require(sceAudioOutGetPortState(hAux, &state) == 0);
        Require(state.output == 0x80);
        Require(state.channel == 0);
        Require(sceAudioOutClose(hAux) == 0);
    }
}

void Test71ChannelSetVolumeAndRemapBounds() {
    Require(sceAudioOutSetVolume(0, 0xFF, nullptr) == errorInvalidPointer);
    Require(sceAudioOutSetVolume(-1, 0xFF, nullptr) == errorInvalidPointer);
    Require(sceAudioOutSetVolume(33, 0xFF, nullptr) == errorInvalidPointer);

    std::array<int, 8> volumes = {1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000};
    Require(sceAudioOutSetVolume(0, 0xFF, volumes.data()) == errorInvalidPort);
    Require(sceAudioOutSetVolume(-1, 0xFF, volumes.data()) == errorInvalidPort);
    Require(sceAudioOutSetVolume(33, 0xFF, volumes.data()) == errorInvalidPort);

    for (const auto fmt : channel71Formats) {
        const int h = sceAudioOutOpen(testUser, 0, 0, 256, 48000, fmt);
        Require(h > 0);

        Require(sceAudioOutSetVolume(h, 0xFF, nullptr) == errorInvalidPointer);

        Require(sceAudioOutSetVolume(h, 0x00, volumes.data()) == 0);
        Require(sceAudioOutSetVolume(h, 0x01, volumes.data()) == 0);
        Require(sceAudioOutSetVolume(h, 0x02, volumes.data()) == 0);
        Require(sceAudioOutSetVolume(h, 0x04, volumes.data()) == 0);
        Require(sceAudioOutSetVolume(h, 0x08, volumes.data()) == 0);
        Require(sceAudioOutSetVolume(h, 0x10, volumes.data()) == 0);
        Require(sceAudioOutSetVolume(h, 0x20, volumes.data()) == 0);
        Require(sceAudioOutSetVolume(h, 0x40, volumes.data()) == 0);
        Require(sceAudioOutSetVolume(h, 0x80, volumes.data()) == 0);
        Require(sceAudioOutSetVolume(h, 0x55, volumes.data()) == 0);
        Require(sceAudioOutSetVolume(h, 0xAA, volumes.data()) == 0);
        Require(sceAudioOutSetVolume(h, 0xFF, volumes.data()) == 0);
        Require(sceAudioOutSetVolume(h, 0x1FF, volumes.data()) == 0);
        Require(sceAudioOutSetVolume(h, 0xFFFF, volumes.data()) == 0);

        Require(sceAudioOutClose(h) == 0);
        Require(sceAudioOutSetVolume(h, 0xFF, volumes.data()) == errorInvalidPort);
    }
}

void Test71ChannelOutputAndTimingBounds() {
    Require(sceAudioOutOutput(0, nullptr) == errorInvalidPort);
    Require(sceAudioOutOutput(-1, nullptr) == errorInvalidPort);
    Require(sceAudioOutOutput(33, nullptr) == errorInvalidPort);

    std::uint64_t lastTime = 12345;
    Require(sceAudioOutGetLastOutputTime(0, nullptr) == errorInvalidPointer);
    Require(sceAudioOutGetLastOutputTime(-1, nullptr) == errorInvalidPointer);
    Require(sceAudioOutGetLastOutputTime(0, &lastTime) == errorInvalidPort);
    Require(sceAudioOutGetLastOutputTime(-1, &lastTime) == errorInvalidPort);
    Require(sceAudioOutGetLastOutputTime(33, &lastTime) == errorInvalidPort);

    for (const auto fmt : channel71Formats) {
        const int h = sceAudioOutOpen(testUser, 0, 0, 256, 48000, fmt);
        Require(h > 0);

        lastTime = 999;
        Require(sceAudioOutGetLastOutputTime(h, &lastTime) == 0);
        Require(lastTime == 0);

        Require(sceAudioOutOutput(h, nullptr) == 256);
        lastTime = 999;
        Require(sceAudioOutGetLastOutputTime(h, &lastTime) == 0);
        Require(lastTime == 0);

        if (fmt == formatS16_8Ch || fmt == formatS16_8ChStd) {
            std::vector<std::int16_t> s16Buffer(256 * 8, 1234);
            Require(sceAudioOutOutput(h, s16Buffer.data()) == 256);
        } else {
            std::vector<float> f32Buffer(256 * 8, 0.5f);
            Require(sceAudioOutOutput(h, f32Buffer.data()) == 256);
        }

        lastTime = 0;
        Require(sceAudioOutGetLastOutputTime(h, &lastTime) == 0);
        Require(lastTime > 0);

        Require(sceAudioOutClose(h) == 0);
        Require(sceAudioOutOutput(h, nullptr) == errorInvalidPort);
        Require(sceAudioOutGetLastOutputTime(h, &lastTime) == errorInvalidPort);
    }
}

void Test71ChannelMultiPortOutputsBounds() {
    AudioOutOutputParam validParam{};
    validParam.handle = 1;
    validParam.ptr = nullptr;

    Require(sceAudioOutOutputs(nullptr, 1) == errorInvalidPointer);
    Require(sceAudioOutOutputs(&validParam, 0) == errorInvalidPointer);
    Require(sceAudioOutOutputs(nullptr, 0) == errorInvalidPointer);

    const int h1 = sceAudioOutOpen(testUser, 0, 0, 256, 48000, formatS16_8Ch);
    const int h2 = sceAudioOutOpen(testUser, 1, 0, 512, 48000, formatF32_8ChStd);
    Require(h1 > 0 && h2 > 0);

    AudioOutOutputParam params[2]{};
    params[0].handle = h1;
    params[0].ptr = nullptr;
    params[1].handle = h2;
    params[1].ptr = nullptr;

    Require(sceAudioOutOutputs(params, 2) == 256);

    std::vector<std::int16_t> buf1(256 * 8, 500);
    std::vector<float> buf2(512 * 8, 0.25f);
    params[0].ptr = buf1.data();
    params[1].ptr = buf2.data();
    Require(sceAudioOutOutputs(params, 2) == 256);

    params[1].handle = 0;
    Require(sceAudioOutOutputs(params, 2) == errorInvalidPort);

    params[1].handle = -1;
    Require(sceAudioOutOutputs(params, 2) == errorInvalidPort);

    params[1].handle = 33;
    Require(sceAudioOutOutputs(params, 2) == errorInvalidPort);

    Require(sceAudioOutClose(h1) == 0);
    Require(sceAudioOutClose(h2) == 0);
}

void Test71ChannelCapacityAndLifecycleBounds() {
    std::array<int, 32> ports{};
    for (int i = 0; i < 32; ++i) {
        const std::uint32_t fmt = channel71Formats[i % channel71Formats.size()];
        ports[i] = sceAudioOutOpen(testUser, 0, 0, 256, 48000, fmt);
        Require(ports[i] >= 1 && ports[i] <= 32);
    }

    const int overflow = sceAudioOutOpen(testUser, 0, 0, 256, 48000, formatS16_8ChStd);
    Require(overflow == errorPortFull);

    const int freed = ports[7];
    Require(sceAudioOutClose(freed) == 0);
    Require(sceAudioOutClose(freed) == errorInvalidPort);

    const int recycled = sceAudioOutOpen(testUser, 0, 0, 256, 48000, formatF32_8Ch);
    Require(recycled == freed);
    ports[7] = recycled;

    for (int i = 0; i < 32; ++i) {
        Require(sceAudioOutClose(ports[i]) == 0);
        Require(sceAudioOutClose(ports[i]) == errorInvalidPort);
    }

    const int fresh = sceAudioOutOpen(testUser, 0, 0, 256, 48000, formatS16_8Ch);
    Require(fresh >= 1 && fresh <= 32);
    Require(sceAudioOutClose(fresh) == 0);
}

} // namespace

int main() {
    TestAudioOutInit();
    Test71ChannelOpenParameterBounds();
    Test71ChannelPortStateBounds();
    Test71ChannelSetVolumeAndRemapBounds();
    Test71ChannelOutputAndTimingBounds();
    Test71ChannelMultiPortOutputsBounds();
    Test71ChannelCapacityAndLifecycleBounds();

    LibcRunShutdown_nid_postfix();
    return 0;
}
