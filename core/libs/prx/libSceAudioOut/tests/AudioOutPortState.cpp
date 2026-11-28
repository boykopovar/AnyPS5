#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

namespace {

constexpr int user = 0x10000000;
constexpr int portTypeMain = 0;
constexpr std::uint32_t formatS16Stereo = 1;
constexpr std::uint32_t frames = 1024;
constexpr std::uint32_t frequency = 48000;
constexpr std::uint8_t channelsOfStereoPort = 2;

AudioOutPortState StateOf(int handle) {
    AudioOutPortState state{};
    state.channel = 0xA5;
    state.flag = ~0ull;
    state.activeState = ~0ull;
    Require(sceAudioOutGetPortState(handle, &state) == 0, "an open port must report its state");
    return state;
}

void CheckStereoMainState(const AudioOutPortState& state) {
    Require(state.channel == channelsOfStereoPort, "a stereo main port must report 2 channels");
    Require(state.flag == 0, "the port flag must be 0");
    Require(state.activeState == 0, "the active state must be 0");
}

}

int main() {
    const int handle = sceAudioOutOpen(user, portTypeMain, 0, frames, frequency, formatS16Stereo);
    Require(handle > 0, "port must open");
    CheckStereoMainState(StateOf(handle));

    const std::vector<std::int16_t> silence(static_cast<std::size_t>(frames) * 2, 0);
    Require(sceAudioOutOutput(handle, silence.data()) == static_cast<int>(frames), "output must accept the block");
    CheckStereoMainState(StateOf(handle));

    Require(sceAudioOutClose(handle) == 0, "port must close");
    return 0;
}
