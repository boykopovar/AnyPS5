#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace {

constexpr int testUser = 0x10000000;
constexpr int errorInvalidPort = -2144993277;
constexpr int errorInvalidPointer = -2144993276;
constexpr int errorPortFull = -2144993275;
constexpr int errorAlreadyInitialized = static_cast<int>(0x8026000e);

constexpr int portTypeMain = 0;
constexpr int portTypeBgm = 1;
constexpr int portTypeVoice = 2;
constexpr int portTypePersonal = 3;
constexpr int portTypePadSpk = 4;
constexpr int portTypeAudio3d = 126;
constexpr int portTypeAux = 127;

constexpr std::uint32_t formatS16Mono = 0;
constexpr std::uint32_t formatS16Stereo = 1;
constexpr std::uint32_t formatS16_8Ch = 2;
constexpr std::uint32_t formatF32Mono = 3;
constexpr std::uint32_t formatF32Stereo = 4;
constexpr std::uint32_t formatF32_8Ch = 5;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestAudioOutInit() {
    const int rc = sceAudioOutInit();
    Require(rc == 0 || rc == errorAlreadyInitialized);
}

void TestPortStatePointerPrecedenceAndInvalidHandles() {
    Require(sceAudioOutGetPortState(0, nullptr) == errorInvalidPointer);
    Require(sceAudioOutGetPortState(-1, nullptr) == errorInvalidPointer);
    Require(sceAudioOutGetPortState(-9999, nullptr) == errorInvalidPointer);
    Require(sceAudioOutGetPortState(1, nullptr) == errorInvalidPointer);
    Require(sceAudioOutGetPortState(32, nullptr) == errorInvalidPointer);
    Require(sceAudioOutGetPortState(33, nullptr) == errorInvalidPointer);
    Require(sceAudioOutGetPortState(std::numeric_limits<int>::min(), nullptr) == errorInvalidPointer);
    Require(sceAudioOutGetPortState(std::numeric_limits<int>::max(), nullptr) == errorInvalidPointer);

    AudioOutPortState state{};
    Require(sceAudioOutGetPortState(0, &state) == errorInvalidPort);
    Require(sceAudioOutGetPortState(-1, &state) == errorInvalidPort);
    Require(sceAudioOutGetPortState(-100, &state) == errorInvalidPort);
    Require(sceAudioOutGetPortState(33, &state) == errorInvalidPort);
    Require(sceAudioOutGetPortState(100, &state) == errorInvalidPort);
    Require(sceAudioOutGetPortState(std::numeric_limits<int>::min(), &state) == errorInvalidPort);
    Require(sceAudioOutGetPortState(std::numeric_limits<int>::max(), &state) == errorInvalidPort);

    for (int handle = 1; handle <= 32; ++handle) {
        Require(sceAudioOutGetPortState(handle, &state) == errorInvalidPort);
    }
}

void VerifyPortStateBasics(const AudioOutPortState& state, std::uint16_t expectedOutput, std::uint8_t expectedChannel) {
    Require(state.output == expectedOutput);
    Require(state.channel == expectedChannel);
    Require(state.rerouteCounter == 0);
    Require(state.volume == 127);
    Require(state.flag == 0);
    Require(state.activeState == 0);
    Require(state.reserved[0] == 0);
}

void TestPortStateAcrossPortTypesAndFormats() {
    struct TestCase {
        int type;
        std::uint32_t format;
        std::uint16_t expectedOutput;
        std::uint8_t expectedChannel;
    };

    const TestCase cases[] = {
        {portTypeMain, formatS16Mono, 1, 1},
        {portTypeMain, formatS16Stereo, 1, 2},
        {portTypeMain, formatS16_8Ch, 1, 2},
        {portTypeMain, formatF32Mono, 1, 1},
        {portTypeMain, formatF32Stereo, 1, 2},
        {portTypeMain, formatF32_8Ch, 1, 2},
        {portTypeBgm, formatS16Stereo, 1, 2},
        {portTypeVoice, formatS16Mono, 0x40, 1},
        {portTypePersonal, formatS16Mono, 0x40, 1},
        {portTypePadSpk, formatS16Mono, 4, 1},
        {portTypeAudio3d, formatS16_8Ch, 1, 2},
        {portTypeAux, formatS16Stereo, 0x80, 0},
    };

    for (const auto& testCase : cases) {
        const int handle = sceAudioOutOpen(testUser, testCase.type, 0, 256, 48000, testCase.format);
        Require(handle >= 1 && handle <= 32);

        Require(sceAudioOutGetPortState(handle, nullptr) == errorInvalidPointer);

        AudioOutPortState state{};
        Require(sceAudioOutGetPortState(handle, &state) == 0);
        VerifyPortStateBasics(state, testCase.expectedOutput, testCase.expectedChannel);

        Require(sceAudioOutClose(handle) == 0);
        Require(sceAudioOutGetPortState(handle, &state) == errorInvalidPort);
        Require(sceAudioOutGetPortState(handle, nullptr) == errorInvalidPointer);
    }
}

void TestPortStateLifecycleAndClosure() {
    const int handle = sceAudioOutOpen(testUser, portTypeMain, 0, 512, 48000, formatS16Stereo);
    Require(handle >= 1 && handle <= 32);

    AudioOutPortState state{};
    Require(sceAudioOutGetPortState(handle, &state) == 0);
    VerifyPortStateBasics(state, 1, 2);

    std::uint64_t lastTime = 12345;
    Require(sceAudioOutGetLastOutputTime(handle, nullptr) == errorInvalidPointer);
    Require(sceAudioOutGetLastOutputTime(handle, &lastTime) == 0);
    Require(lastTime == 0);

    Require(sceAudioOutClose(handle) == 0);
    Require(sceAudioOutClose(handle) == errorInvalidPort);

    Require(sceAudioOutGetPortState(handle, &state) == errorInvalidPort);
    Require(sceAudioOutGetPortState(handle, nullptr) == errorInvalidPointer);
    Require(sceAudioOutGetLastOutputTime(handle, &lastTime) == errorInvalidPort);

    const int reused = sceAudioOutOpen(testUser, portTypeVoice, 0, 256, 48000, formatS16Mono);
    Require(reused == handle);

    Require(sceAudioOutGetPortState(reused, &state) == 0);
    VerifyPortStateBasics(state, 0x40, 1);

    Require(sceAudioOutClose(reused) == 0);
    Require(sceAudioOutGetPortState(reused, &state) == errorInvalidPort);
}

void TestPortStateCapacityExhaustion() {
    std::array<int, 32> ports{};
    for (int i = 0; i < 32; ++i) {
        ports[i] = sceAudioOutOpen(testUser, portTypeMain, 0, 256, 48000, formatS16Stereo);
        Require(ports[i] >= 1 && ports[i] <= 32);

        AudioOutPortState state{};
        Require(sceAudioOutGetPortState(ports[i], &state) == 0);
        VerifyPortStateBasics(state, 1, 2);
    }

    const int overflow = sceAudioOutOpen(testUser, portTypeMain, 0, 256, 48000, formatS16Stereo);
    Require(overflow == errorPortFull);

    AudioOutPortState state{};
    Require(sceAudioOutGetPortState(33, &state) == errorInvalidPort);

    const int freed = ports[15];
    Require(sceAudioOutClose(freed) == 0);
    Require(sceAudioOutGetPortState(freed, &state) == errorInvalidPort);

    for (int i = 0; i < 32; ++i) {
        if (i == 15) {
            continue;
        }
        Require(sceAudioOutGetPortState(ports[i], &state) == 0);
        VerifyPortStateBasics(state, 1, 2);
    }

    const int recycled = sceAudioOutOpen(testUser, portTypeMain, 0, 256, 48000, formatS16Stereo);
    Require(recycled == freed);
    ports[15] = recycled;

    Require(sceAudioOutGetPortState(recycled, &state) == 0);
    VerifyPortStateBasics(state, 1, 2);

    for (int i = 0; i < 32; ++i) {
        Require(sceAudioOutClose(ports[i]) == 0);
        Require(sceAudioOutGetPortState(ports[i], &state) == errorInvalidPort);
    }
}

void TestSystemStateBounds() {
    Require(sceAudioOutGetSystemState(nullptr) == errorInvalidPointer);

    AudioOutSystemState sysState{};
    sysState.loudness = 1.0f;
    Require(sceAudioOutGetSystemState(&sysState) == 0);
    Require(sysState.loudness == 0.0f);
}

} // namespace

int main() {
    TestAudioOutInit();
    TestPortStatePointerPrecedenceAndInvalidHandles();
    TestPortStateAcrossPortTypesAndFormats();
    TestPortStateLifecycleAndClosure();
    TestPortStateCapacityExhaustion();
    TestSystemStateBounds();

    LibcRunShutdown_nid_postfix();
    return 0;
}
