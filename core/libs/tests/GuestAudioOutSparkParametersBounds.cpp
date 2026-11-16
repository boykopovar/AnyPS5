#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "prx/libSceAudioOut/include/LaterExports.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace {

constexpr int audioOutErrorInvalidPort = static_cast<int>(0x80260003);
constexpr int audioOutErrorInvalidPointer = static_cast<int>(0x80260004);
constexpr int simulatedHandleStatusConst = static_cast<int>(0x809B00FF);
constexpr int userId = 0x10000000;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename TAction>
bool Throws(TAction&& action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void TestSparkEffectAndUnimplementedStubs() {
    Require(Throws([&] { sceAudioOutSetSparkEQ(); }));
    Require(Throws([&] { sceAudioOut2MbusSetSparkParam(); }));
    Require(Throws([&] { sceAudioOut2SetSparkEQ(); }));

    Require(Throws([&] { sceAudioOut2MbusSetMorpheusParam(); }));
    Require(Throws([&] { sceAudioOut2MbusSetCaesarParam(); }));
    Require(Throws([&] { sceAudioOutSetCaesarVolume(); }));
    Require(Throws([&] { sceAudioOutSetUsbVolume(); }));
    Require(Throws([&] { sceAudioOutSetPadJackVolume(); }));
    Require(Throws([&] { sceAudioOutSetPadSpkVolume(1, 100); }));
    Require(Throws([&] { sceAudioOutSetHeadphoneOutMode(); }));
    Require(Throws([&] { sceAudioOutSetChatBalance(); }));
    Require(Throws([&] { sceAudioOutSetVolumeDown(); }));
    Require(Throws([&] { sceAudioOutSetRecMode(); }));
    Require(Throws([&] { sceAudioOutSetAllMute(); }));
    Require(Throws([&] { sceAudioOutSetSystemDebugState(); }));
    Require(Throws([&] { sceAudioOutArbitrationInitialize(); }));
    Require(Throws([&] { sceAudioOut2ArbitrationInitialize(); }));
    Require(Throws([&] { sceAudioOutEnable3DAudioByArbitration(); }));
    Require(Throws([&] { sceAudioOutEnable3DAudioForUser(); }));
    Require(Throws([&] { sceAudioOutEnableTVVirtual(); }));
    Require(Throws([&] { sceAudioOutEnableTVCorrection(); }));
    Require(Throws([&] { sceAudioOutEnableMonoHpOutput(); }));
    Require(Throws([&] { sceAudioOutEnableVrControllerLowPowerMode(); }));
    Require(Throws([&] { sceAudioOutEnableCinematicVrRotation(); }));
    Require(Throws([&] { sceAudioOutCustomHrtfIsExist(); }));
}

void TestInformationAndSimulatedHandleStatus() {
    Require(sceAudioOutGetSimulatedHandleStatusInfo() == simulatedHandleStatusConst);
    Require(sceAudioOutGetInfoOpenNum() == 0);
    Require(sceAudioOutGetInfo() == 0);
}

void TestUnopenedPortGatesAndPrecedence() {
    int vol[8] = {32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768};

    for (const int unopenedHandle : {-100, -1, 0, 1, 2, 8, 16, 32, 33, 100}) {
        Require(sceAudioOutClose(unopenedHandle) == audioOutErrorInvalidPort);
        Require(sceAudioOutOutput(unopenedHandle, nullptr) == audioOutErrorInvalidPort);

        AudioOutPortState state{};
        Require(sceAudioOutGetPortState(unopenedHandle, &state) == audioOutErrorInvalidPort);
        Require(sceAudioOutGetPortState(unopenedHandle, nullptr) == audioOutErrorInvalidPointer);

        std::uint64_t outTime = 0;
        Require(sceAudioOutGetLastOutputTime(unopenedHandle, &outTime) == audioOutErrorInvalidPort);
        Require(sceAudioOutGetLastOutputTime(unopenedHandle, nullptr) == audioOutErrorInvalidPointer);

        Require(sceAudioOutSetVolume(unopenedHandle, 0xFF, nullptr) == audioOutErrorInvalidPointer);
        Require(sceAudioOutSetVolume(unopenedHandle, 0xFF, vol) == audioOutErrorInvalidPort);

        Require(sceAudioOutSetMixLevelPadSpk(unopenedHandle, 11626) == audioOutErrorInvalidPort);
    }
}

void TestPortLifecycleAndPostCloseValidation() {
    const int port = sceAudioOutOpen(userId, 0, 0, 256, 48000, 1);
    Require(port > 0);

    AudioOutPortState state{};
    Require(sceAudioOutGetPortState(port, &state) == 0);

    int vol[8] = {32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768};
    Require(sceAudioOutSetVolume(port, 0x3, vol) == 0);

    Require(sceAudioOutClose(port) == 0);

    Require(sceAudioOutClose(port) == audioOutErrorInvalidPort);
    Require(sceAudioOutOutput(port, nullptr) == audioOutErrorInvalidPort);
    Require(sceAudioOutGetPortState(port, &state) == audioOutErrorInvalidPort);
    Require(sceAudioOutGetPortState(port, nullptr) == audioOutErrorInvalidPointer);

    std::uint64_t outTime = 0;
    Require(sceAudioOutGetLastOutputTime(port, &outTime) == audioOutErrorInvalidPort);
    Require(sceAudioOutGetLastOutputTime(port, nullptr) == audioOutErrorInvalidPointer);

    Require(sceAudioOutSetVolume(port, 0xFF, nullptr) == audioOutErrorInvalidPointer);
    Require(sceAudioOutSetVolume(port, 0xFF, vol) == audioOutErrorInvalidPort);

    Require(sceAudioOutSetMixLevelPadSpk(port, 11626) == audioOutErrorInvalidPort);
}

} // namespace

int main() {
    Require(sceAudioOutInit() == 0 || sceAudioOutInit() == static_cast<int>(0x8026000E));
    Require(sceAudioOut2Initialize() == 0 || sceAudioOut2Initialize() == static_cast<int>(0x80268004));

    TestSparkEffectAndUnimplementedStubs();
    TestInformationAndSimulatedHandleStatus();
    TestUnopenedPortGatesAndPrecedence();
    TestPortLifecycleAndPostCloseValidation();

    LibcRunShutdown_nid_postfix();
    return 0;
}
