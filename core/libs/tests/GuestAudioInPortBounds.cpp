#include "prx/libSceAudioIn/include/AudioIn.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>

static void Require(bool value) { if (!value) std::abort(); }

namespace {

constexpr int user = 0x10000000;
constexpr int invalidSize = static_cast<int>(0x80260102);
constexpr int invalidFreq = static_cast<int>(0x80260103);
constexpr int invalidType = static_cast<int>(0x80260104);
constexpr int invalidParam = static_cast<int>(0x80260106);

void TestPortBoundsAndChannels() {
    Require(sceAudioInInit() == 0);
    Require(sceAudioInOpen(user, 6, 0, 256, 48000, 2) == invalidType);
    Require(sceAudioInOpen(user, 0, 1, 256, 48000, 2) == invalidParam);
    Require(sceAudioInOpen(user, 0, 0, 512, 48000, 2) == invalidSize);
    Require(sceAudioInOpen(user, 0, 0, 256, 44100, 2) == invalidFreq);
    Require(sceAudioInOpen(user, 0, 0, 256, 48000, 3) == invalidParam);
    Require(sceAudioInOpen(user, 0, 0, 256, 48000, 0x20) == invalidParam);

    int port = sceAudioInOpen(user, 0, 0, 256, 48000, 2);
    Require(port > 0);
    Require(sceAudioInClose(port) == 0);
}

} // namespace

int main() {
    TestPortBoundsAndChannels();
    return 0;
}
