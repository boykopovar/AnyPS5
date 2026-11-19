#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

constexpr int user = 0x10000000;
constexpr int errorInvalidPortType = static_cast<int>(0x8026000A);
constexpr int errorInvalidPort = static_cast<int>(0x80260003);
constexpr int errorInvalidPointer = static_cast<int>(0x80260004);
constexpr int errorPortFull = static_cast<int>(0x80260005);
constexpr int errorInvalidSize = static_cast<int>(0x80260006);
constexpr int errorInvalidFormat = static_cast<int>(0x80260007);
constexpr int errorInvalidFreq = static_cast<int>(0x80260008);
constexpr int errorInvalidLevel = static_cast<int>(0x80260014);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestAudioOutInit() {
    Require(sceAudioOutInit() == 0 || sceAudioOutInit() == static_cast<int>(0x8026000e));
}

void TestPortOpenParameterBounds() {
    Require(sceAudioOutOpen(user, -1, 0, 256, 48000, 1) == errorInvalidPortType);
    Require(sceAudioOutOpen(user, 5, 0, 256, 48000, 1) == errorInvalidPortType);
    Require(sceAudioOutOpen(user, 6, 0, 256, 48000, 1) == errorInvalidPortType);
    Require(sceAudioOutOpen(user, 7, 0, 256, 48000, 1) == errorInvalidPortType);
    Require(sceAudioOutOpen(user, 8, 0, 256, 48000, 1) == errorInvalidPortType);
    Require(sceAudioOutOpen(user, 9, 0, 256, 48000, 1) == errorInvalidPortType);
    Require(sceAudioOutOpen(user, 11, 0, 256, 48000, 1) == errorInvalidPortType);
    Require(sceAudioOutOpen(user, 125, 0, 256, 48000, 1) == errorInvalidPortType);
    Require(sceAudioOutOpen(user, 128, 0, 256, 48000, 1) == errorInvalidPortType);

    Require(sceAudioOutOpen(user, -1, 0, 0, 0, 1) == errorInvalidPortType);

    Require(sceAudioOutOpen(user, 0, 0, 0, 48000, 1) == errorInvalidSize);
    Require(sceAudioOutOpen(user, 0, 0, 0, 0, 1) == errorInvalidSize);

    Require(sceAudioOutOpen(user, 0, 0, 256, 0, 1) == errorInvalidFreq);
    Require(sceAudioOutOpen(user, 0, 0, 256, 0, 0xFF) == errorInvalidFreq);

    Require(sceAudioOutOpen(user, 0, 0, 256, 48000, 8) == errorInvalidFormat);
    Require(sceAudioOutOpen(user, 0, 0, 256, 48000, 9) == errorInvalidFormat);
    Require(sceAudioOutOpen(user, 0, 0, 256, 48000, 0x1F) == errorInvalidFormat);
    Require(sceAudioOutOpen(user, 0, 0, 256, 48000, 0xFF) == errorInvalidFormat);

    const int validExtraBits = sceAudioOutOpen(user, 0, 0, 256, 48000, 0x100 | 1);
    Require(validExtraBits > 0 && validExtraBits <= 32);
    Require(sceAudioOutClose(validExtraBits) == 0);

    bool vibrationThrew = false;
    try {
        sceAudioOutOpen(user, 10, 0, 256, 48000, 2);
    } catch (const std::runtime_error&) {
        vibrationThrew = true;
    }
    Require(vibrationThrew);
}

void TestPortHandleValidationAndDoubleClose() {
    Require(sceAudioOutClose(0) == errorInvalidPort);
    Require(sceAudioOutClose(-1) == errorInvalidPort);
    Require(sceAudioOutClose(-100) == errorInvalidPort);
    Require(sceAudioOutClose(33) == errorInvalidPort);
    Require(sceAudioOutClose(999) == errorInvalidPort);
    Require(sceAudioOutClose(0x7fffffff) == errorInvalidPort);

    const int h = sceAudioOutOpen(user, 0, 0, 256, 48000, 1);
    Require(h >= 1 && h <= 32);
    Require(sceAudioOutClose(h) == 0);
    Require(sceAudioOutClose(h) == errorInvalidPort);

    Require(sceAudioOutOutput(h, nullptr) == errorInvalidPort);
    AudioOutPortState state{};
    Require(sceAudioOutGetPortState(h, &state) == errorInvalidPort);
    int vol[8]{};
    Require(sceAudioOutSetVolume(h, 1, vol) == errorInvalidPort);
    std::uint64_t lastTime = 0;
    Require(sceAudioOutGetLastOutputTime(h, &lastTime) == errorInvalidPort);
    Require(sceAudioOutSetMixLevelPadSpk(h, 1000) == errorInvalidPort);
}

void TestPointerFaultRejectionAndPrecedence() {
    Require(sceAudioOutGetPortState(-1, nullptr) == errorInvalidPointer);
    Require(sceAudioOutGetPortState(0, nullptr) == errorInvalidPointer);
    Require(sceAudioOutGetPortState(999, nullptr) == errorInvalidPointer);

    Require(sceAudioOutSetVolume(-1, 1, nullptr) == errorInvalidPointer);
    Require(sceAudioOutSetVolume(0, 1, nullptr) == errorInvalidPointer);
    Require(sceAudioOutSetVolume(999, 1, nullptr) == errorInvalidPointer);

    Require(sceAudioOutGetLastOutputTime(-1, nullptr) == errorInvalidPointer);
    Require(sceAudioOutGetLastOutputTime(0, nullptr) == errorInvalidPointer);
    Require(sceAudioOutGetLastOutputTime(999, nullptr) == errorInvalidPointer);

    Require(sceAudioOutOutputs(nullptr, 1) == errorInvalidPointer);
    AudioOutOutputParam dummyParam{};
    Require(sceAudioOutOutputs(&dummyParam, 0) == errorInvalidPointer);
    Require(sceAudioOutOutputs(nullptr, 0) == errorInvalidPointer);

    Require(sceAudioOutGetSystemState(nullptr) == errorInvalidPointer);
    AudioOutSystemState sysState{};
    Require(sceAudioOutGetSystemState(&sysState) == 0);
    Require(sysState.loudness == 0.0f);

    const int port = sceAudioOutOpen(user, 0, 0, 256, 48000, 1);
    Require(port >= 1 && port <= 32);
    Require(sceAudioOutGetPortState(port, nullptr) == errorInvalidPointer);
    Require(sceAudioOutSetVolume(port, 1, nullptr) == errorInvalidPointer);
    Require(sceAudioOutGetLastOutputTime(port, nullptr) == errorInvalidPointer);
    Require(sceAudioOutClose(port) == 0);
}

void TestPortStateAcrossTypes() {
    AudioOutPortState state{};

    const int hMain = sceAudioOutOpen(user, 0, 0, 256, 48000, 1);
    Require(hMain > 0);
    Require(sceAudioOutGetPortState(hMain, &state) == 0);
    Require(state.output == 1);
    Require(state.channel == 2);
    Require(state.volume == 127);
    Require(state.rerouteCounter == 0);
    Require(state.flag == 0);
    Require(state.activeState == 0);
    Require(state.reserved[0] == 0);
    Require(sceAudioOutClose(hMain) == 0);

    const int hBgm = sceAudioOutOpen(user, 1, 0, 256, 48000, 1);
    Require(hBgm > 0);
    Require(sceAudioOutGetPortState(hBgm, &state) == 0);
    Require(state.output == 1);
    Require(state.channel == 2);
    Require(sceAudioOutClose(hBgm) == 0);

    const int h3d = sceAudioOutOpen(user, 126, 0, 256, 48000, 1);
    Require(h3d > 0);
    Require(sceAudioOutGetPortState(h3d, &state) == 0);
    Require(state.output == 1);
    Require(state.channel == 2);
    Require(sceAudioOutClose(h3d) == 0);

    const int hVoice = sceAudioOutOpen(user, 2, 0, 256, 48000, 0);
    Require(hVoice > 0);
    Require(sceAudioOutGetPortState(hVoice, &state) == 0);
    Require(state.output == 0x40);
    Require(state.channel == 1);
    Require(sceAudioOutClose(hVoice) == 0);

    const int hPersonal = sceAudioOutOpen(user, 3, 0, 256, 48000, 1);
    Require(hPersonal > 0);
    Require(sceAudioOutGetPortState(hPersonal, &state) == 0);
    Require(state.output == 0x40);
    Require(state.channel == 1);
    Require(sceAudioOutClose(hPersonal) == 0);

    const int hPad = sceAudioOutOpen(user, 4, 0, 256, 48000, 0);
    Require(hPad > 0);
    Require(sceAudioOutGetPortState(hPad, &state) == 0);
    Require(state.output == 4);
    Require(state.channel == 1);
    Require(sceAudioOutClose(hPad) == 0);

    const int hAux = sceAudioOutOpen(user, 127, 0, 256, 48000, 0);
    Require(hAux > 0);
    Require(sceAudioOutGetPortState(hAux, &state) == 0);
    Require(state.output == 0x80);
    Require(state.channel == 0);
    Require(sceAudioOutClose(hAux) == 0);
}

void TestSetVolumeAndStdChannelRemap() {
    const int h8 = sceAudioOutOpen(user, 0, 0, 256, 48000, 6);
    Require(h8 > 0);

    std::array<int, 8> volumes = {1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000};
    Require(sceAudioOutSetVolume(h8, 0xFF, volumes.data()) == 0);
    Require(sceAudioOutSetVolume(h8, 0x03, volumes.data()) == 0);
    Require(sceAudioOutSetVolume(h8, 0x00, volumes.data()) == 0);

    Require(sceAudioOutClose(h8) == 0);
}

void TestMixLevelPadSpkBounds() {
    const int hPad = sceAudioOutOpen(user, 4, 0, 256, 48000, 0);
    Require(hPad > 0);

    const int hMain = sceAudioOutOpen(user, 0, 0, 256, 48000, 1);
    Require(hMain > 0);

    Require(sceAudioOutSetMixLevelPadSpk(-1, 50000) == errorInvalidPort);
    Require(sceAudioOutSetMixLevelPadSpk(hMain, 10000) == errorInvalidPortType);

    Require(sceAudioOutSetMixLevelPadSpk(hPad, 0) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(hPad, 11626) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(hPad, 32768) == 0);

    Require(sceAudioOutSetMixLevelPadSpk(hPad, 32769) == errorInvalidLevel);
    Require(sceAudioOutSetMixLevelPadSpk(hPad, 50000) == errorInvalidLevel);
    Require(sceAudioOutSetMixLevelPadSpk(hPad, 0xFFFFFFFFu) == errorInvalidLevel);

    Require(sceAudioOutClose(hPad) == 0);
    Require(sceAudioOutClose(hMain) == 0);
}

void TestAudioOutOutputAndBufferPacing() {
    const int h = sceAudioOutOpen(user, 0, 0, 256, 48000, 1);
    Require(h > 0);

    std::uint64_t initialTime = 999;
    Require(sceAudioOutGetLastOutputTime(h, &initialTime) == 0);
    Require(initialTime == 0);

    Require(sceAudioOutOutput(h, nullptr) == 256);
    std::uint64_t nullTime = 999;
    Require(sceAudioOutGetLastOutputTime(h, &nullTime) == 0);
    Require(nullTime == 0);

    std::vector<std::int16_t> sampleData(256 * 2, 0x1234);
    Require(sceAudioOutOutput(h, sampleData.data()) == 256);

    std::uint64_t afterTime = 0;
    Require(sceAudioOutGetLastOutputTime(h, &afterTime) == 0);
    Require(afterTime > 0);

    Require(sceAudioOutClose(h) == 0);
}

void TestAudioOutOutputsMultiPortValidation() {
    const int h1 = sceAudioOutOpen(user, 0, 0, 256, 48000, 1);
    const int h2 = sceAudioOutOpen(user, 1, 0, 512, 48000, 1);
    Require(h1 > 0 && h2 > 0);

    AudioOutOutputParam params[2]{};
    params[0].handle = h1;
    params[0].ptr = nullptr;
    params[1].handle = h2;
    params[1].ptr = nullptr;

    Require(sceAudioOutOutputs(params, 2) == 256);

    params[1].handle = -1;
    Require(sceAudioOutOutputs(params, 2) == errorInvalidPort);
    params[1].handle = 0;
    Require(sceAudioOutOutputs(params, 2) == errorInvalidPort);
    params[1].handle = 33;
    Require(sceAudioOutOutputs(params, 2) == errorInvalidPort);
    params[1].handle = 999;
    Require(sceAudioOutOutputs(params, 2) == errorInvalidPort);

    params[1].handle = h2;
    std::vector<std::int16_t> buf1(256 * 2, 0x1111);
    std::vector<std::int16_t> buf2(512 * 2, 0x2222);
    params[0].ptr = buf1.data();
    params[1].ptr = buf2.data();
    Require(sceAudioOutOutputs(params, 2) == 256);

    Require(sceAudioOutClose(h1) == 0);
    Require(sceAudioOutClose(h2) == 0);
}

void TestPortCapacityAndRecycling() {
    std::array<int, 32> ports{};
    for (int i = 0; i < 32; ++i) {
        ports[i] = sceAudioOutOpen(user, 0, 0, 256, 48000, 1);
        Require(ports[i] >= 1 && ports[i] <= 32);
    }

    const int excess = sceAudioOutOpen(user, 0, 0, 256, 48000, 1);
    Require(excess == errorPortFull);

    Require(sceAudioOutClose(ports[10]) == 0);

    const int recycled = sceAudioOutOpen(user, 0, 0, 256, 48000, 1);
    Require(recycled == ports[10]);

    ports[10] = recycled;
    for (int i = 0; i < 32; ++i) {
        Require(sceAudioOutClose(ports[i]) == 0);
    }

    const int fresh = sceAudioOutOpen(user, 0, 0, 256, 48000, 1);
    Require(fresh > 0);
    Require(sceAudioOutClose(fresh) == 0);
}

} // namespace

int main() {
    TestAudioOutInit();
    TestPortOpenParameterBounds();
    TestPortHandleValidationAndDoubleClose();
    TestPointerFaultRejectionAndPrecedence();
    TestPortStateAcrossTypes();
    TestSetVolumeAndStdChannelRemap();
    TestMixLevelPadSpkBounds();
    TestAudioOutOutputAndBufferPacing();
    TestAudioOutOutputsMultiPortValidation();
    TestPortCapacityAndRecycling();

    LibcRunShutdown_nid_postfix();
    return 0;
}
