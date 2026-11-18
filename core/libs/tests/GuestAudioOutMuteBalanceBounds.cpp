#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

constexpr int testUser = 0x10000000;
constexpr int errorInvalidPortType = -2144993270;
constexpr int errorInvalidPort = -2144993277;
constexpr int errorInvalidPointer = -2144993276;
constexpr int errorPortFull = -2144993275;
constexpr int errorInvalidLevel = -2144993260;
constexpr int errorAlreadyInitialized = static_cast<int>(0x8026000e);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename F>
void RequireNotImplemented(std::string_view expectedName, F&& func) {
    try {
        func();
        std::abort();
    } catch (const std::runtime_error& err) {
        const std::string_view message = err.what();
        if (message.find(expectedName) == std::string_view::npos ||
            message.find("not implemented") == std::string_view::npos) {
            std::abort();
        }
    }
}

void TestUnimplementedMuteAndBalanceExports() {
    RequireNotImplemented("sceAudioOutSetChatBalance", [] {
        return sceAudioOutSetChatBalance();
    });
    RequireNotImplemented("sceAudioOutSetAllMute", [] {
        return sceAudioOutSetAllMute();
    });
    RequireNotImplemented("sceAudioOutSetVolumeDown", [] {
        return sceAudioOutSetVolumeDown();
    });

    using BalanceFunc = int (APS5_VABI *)();
    using MuteFunc = int (APS5_VABI *)();
    using VolDownFunc = int (APS5_VABI *)();

    const BalanceFunc pBalance = &sceAudioOutSetChatBalance;
    const MuteFunc pMute = &sceAudioOutSetAllMute;
    const VolDownFunc pVolDown = &sceAudioOutSetVolumeDown;

    Require(pBalance != nullptr);
    Require(pMute != nullptr);
    Require(pVolDown != nullptr);

    RequireNotImplemented("sceAudioOutSetChatBalance", [pBalance] {
        return pBalance();
    });
    RequireNotImplemented("sceAudioOutSetAllMute", [pMute] {
        return pMute();
    });
    RequireNotImplemented("sceAudioOutSetVolumeDown", [pVolDown] {
        return pVolDown();
    });

    for (int i = 0; i < 16; ++i) {
        RequireNotImplemented("sceAudioOutSetChatBalance", [] {
            return sceAudioOutSetChatBalance();
        });
        RequireNotImplemented("sceAudioOutSetAllMute", [] {
            return sceAudioOutSetAllMute();
        });
        RequireNotImplemented("sceAudioOutSetVolumeDown", [] {
            return sceAudioOutSetVolumeDown();
        });
    }
}

void TestStereoVolumeBalanceBounds() {
    Require(sceAudioOutSetVolume(0, 0x03, nullptr) == errorInvalidPointer);
    Require(sceAudioOutSetVolume(-1, 0x03, nullptr) == errorInvalidPointer);
    Require(sceAudioOutSetVolume(33, 0x03, nullptr) == errorInvalidPointer);

    std::array<int, 2> dummyVol = {32768, 32768};
    Require(sceAudioOutSetVolume(0, 0x03, dummyVol.data()) == errorInvalidPort);
    Require(sceAudioOutSetVolume(-1, 0x03, dummyVol.data()) == errorInvalidPort);
    Require(sceAudioOutSetVolume(33, 0x03, dummyVol.data()) == errorInvalidPort);

    const int h = sceAudioOutOpen(testUser, 0, 0, 256, 48000, 1);
    Require(h > 0);

    Require(sceAudioOutSetVolume(h, 0x03, nullptr) == errorInvalidPointer);

    std::array<int, 2> muteVol = {0, 0};
    Require(sceAudioOutSetVolume(h, 0x03, muteVol.data()) == 0);

    std::array<int, 2> leftPan = {32768, 0};
    Require(sceAudioOutSetVolume(h, 0x03, leftPan.data()) == 0);

    std::array<int, 2> rightPan = {0, 32768};
    Require(sceAudioOutSetVolume(h, 0x03, rightPan.data()) == 0);

    std::array<int, 2> centeredBalance = {32768, 32768};
    Require(sceAudioOutSetVolume(h, 0x03, centeredBalance.data()) == 0);

    std::array<int, 2> asymmetricBalance = {12000, 28000};
    Require(sceAudioOutSetVolume(h, 0x03, asymmetricBalance.data()) == 0);

    std::array<int, 2> singleChannel = {0, 0};
    Require(sceAudioOutSetVolume(h, 0x01, singleChannel.data()) == 0);
    Require(sceAudioOutSetVolume(h, 0x02, singleChannel.data()) == 0);
    Require(sceAudioOutSetVolume(h, 0x00, centeredBalance.data()) == 0);

    Require(sceAudioOutSetVolume(h, 0xFF, centeredBalance.data()) == 0);
    Require(sceAudioOutSetVolume(h, 0xFFFF, centeredBalance.data()) == 0);

    Require(sceAudioOutClose(h) == 0);
    Require(sceAudioOutSetVolume(h, 0x03, centeredBalance.data()) == errorInvalidPort);
}

void TestSurroundMuteAndChannelBalanceBounds() {
    constexpr std::array<std::uint32_t, 2> surroundFormats = {2, 6};

    for (const auto fmt : surroundFormats) {
        const int h = sceAudioOutOpen(testUser, 0, 0, 256, 48000, fmt);
        Require(h > 0);

        std::array<int, 8> fullMute = {0, 0, 0, 0, 0, 0, 0, 0};
        Require(sceAudioOutSetVolume(h, 0xFF, fullMute.data()) == 0);

        std::array<int, 8> frontOnly = {32768, 32768, 32768, 0, 0, 0, 0, 0};
        Require(sceAudioOutSetVolume(h, 0xFF, frontOnly.data()) == 0);

        std::array<int, 8> surroundOnly = {0, 0, 0, 0, 32768, 32768, 32768, 32768};
        Require(sceAudioOutSetVolume(h, 0xFF, surroundOnly.data()) == 0);

        std::array<int, 8> leftSurroundBalance = {32768, 0, 16384, 0, 32768, 0, 32768, 0};
        Require(sceAudioOutSetVolume(h, 0xFF, leftSurroundBalance.data()) == 0);

        std::array<int, 8> rightSurroundBalance = {0, 32768, 16384, 0, 0, 32768, 0, 32768};
        Require(sceAudioOutSetVolume(h, 0xFF, rightSurroundBalance.data()) == 0);

        std::array<int, 8> centerSubBalance = {0, 0, 32768, 32768, 0, 0, 0, 0};
        Require(sceAudioOutSetVolume(h, 0xFF, centerSubBalance.data()) == 0);

        Require(sceAudioOutSetVolume(h, 0x0F, frontOnly.data()) == 0);
        Require(sceAudioOutSetVolume(h, 0xF0, surroundOnly.data()) == 0);
        Require(sceAudioOutSetVolume(h, 0x55, leftSurroundBalance.data()) == 0);
        Require(sceAudioOutSetVolume(h, 0xAA, rightSurroundBalance.data()) == 0);
        Require(sceAudioOutSetVolume(h, 0x00, fullMute.data()) == 0);

        Require(sceAudioOutClose(h) == 0);
    }
}

void TestPadSpeakerMixLevelMuteBounds() {
    Require(sceAudioOutSetMixLevelPadSpk(0, 0) == errorInvalidPort);
    Require(sceAudioOutSetMixLevelPadSpk(-1, 0) == errorInvalidPort);
    Require(sceAudioOutSetMixLevelPadSpk(33, 0) == errorInvalidPort);

    const int hMain = sceAudioOutOpen(testUser, 0, 0, 256, 48000, 1);
    Require(hMain > 0);
    Require(sceAudioOutSetMixLevelPadSpk(hMain, 0) == errorInvalidPortType);
    Require(sceAudioOutSetMixLevelPadSpk(hMain, 11626) == errorInvalidPortType);
    Require(sceAudioOutClose(hMain) == 0);

    const int hPad = sceAudioOutOpen(testUser, 4, 0, 256, 48000, 1);
    Require(hPad > 0);

    Require(sceAudioOutSetMixLevelPadSpk(hPad, 0) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(hPad, 11626) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(hPad, 16384) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(hPad, 32768) == 0);

    Require(sceAudioOutSetMixLevelPadSpk(hPad, 32769) == errorInvalidLevel);
    Require(sceAudioOutSetMixLevelPadSpk(hPad, 65536) == errorInvalidLevel);
    Require(sceAudioOutSetMixLevelPadSpk(hPad, 0xFFFFFFFFu) == errorInvalidLevel);

    Require(sceAudioOutClose(hPad) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(hPad, 0) == errorInvalidPort);
}

void TestMutedBufferOutputAndPacingBounds() {
    const int h = sceAudioOutOpen(testUser, 0, 0, 256, 48000, 1);
    Require(h > 0);

    std::array<int, 2> muteVol = {0, 0};
    Require(sceAudioOutSetVolume(h, 0x03, muteVol.data()) == 0);

    std::uint64_t lastTime = 999;
    Require(sceAudioOutGetLastOutputTime(h, &lastTime) == 0);
    Require(lastTime == 0);

    Require(sceAudioOutOutput(h, nullptr) == 256);
    lastTime = 999;
    Require(sceAudioOutGetLastOutputTime(h, &lastTime) == 0);
    Require(lastTime == 0);

    std::vector<std::int16_t> samples(256 * 2, 0x1234);
    Require(sceAudioOutOutput(h, samples.data()) == 256);
    lastTime = 0;
    Require(sceAudioOutGetLastOutputTime(h, &lastTime) == 0);
    Require(lastTime > 0);

    std::array<int, 2> rightOnly = {0, 32768};
    Require(sceAudioOutSetVolume(h, 0x03, rightOnly.data()) == 0);
    Require(sceAudioOutOutput(h, samples.data()) == 256);

    Require(sceAudioOutClose(h) == 0);
}

void TestMuteBalanceLifecycleBounds() {
    std::array<int, 32> ports{};
    for (int i = 0; i < 32; ++i) {
        ports[i] = sceAudioOutOpen(testUser, 0, 0, 256, 48000, 1);
        Require(ports[i] >= 1 && ports[i] <= 32);

        std::array<int, 2> vol = {i * 1000, (31 - i) * 1000};
        Require(sceAudioOutSetVolume(ports[i], 0x03, vol.data()) == 0);
    }

    const int overflow = sceAudioOutOpen(testUser, 0, 0, 256, 48000, 1);
    Require(overflow == errorPortFull);

    const int slotToRecycle = ports[5];
    Require(sceAudioOutClose(slotToRecycle) == 0);

    const int reopened = sceAudioOutOpen(testUser, 0, 0, 256, 48000, 1);
    Require(reopened == slotToRecycle);
    ports[5] = reopened;

    for (int i = 0; i < 32; ++i) {
        Require(sceAudioOutClose(ports[i]) == 0);
    }
}

} // namespace

int main() {
    const int rc = sceAudioOutInit();
    Require(rc == 0 || rc == errorAlreadyInitialized);

    TestUnimplementedMuteAndBalanceExports();
    TestStereoVolumeBalanceBounds();
    TestSurroundMuteAndChannelBalanceBounds();
    TestPadSpeakerMixLevelMuteBounds();
    TestMutedBufferOutputAndPacingBounds();
    TestMuteBalanceLifecycleBounds();

    LibcRunShutdown_nid_postfix();
    return 0;
}
