#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

void TestPortStateValidation() {
    Require(sceAudioOutInit() == 0 || sceAudioOutInit() == static_cast<int>(0x8026000e));

    AudioOutPortState state;
    const int nullRes = sceAudioOutGetPortState(1, nullptr);
    Require(nullRes < 0);

    const std::array<int, 6> badHandles = {-100, -1, 0, 999, 12345, 999999};
    for (int handle : badHandles) {
        const int res = sceAudioOutGetPortState(handle, &state);
        Require(res < 0);
    }
}

void TestPortStateTypes() {
    const std::array<int, 5> portTypes = {0, 1, 2, 3, 4};
    for (int type : portTypes) {
        const int port = sceAudioOutOpen(0x10000000, type, 0, 256, 48000, 1);
        if (port > 0) {
            AudioOutPortState state;
            Require(sceAudioOutGetPortState(port, &state) == 0);
            Require(state.volume == 127);
            Require(sceAudioOutClose(port) == 0);
        }
    }
}

void TestPortStateClosedHandle() {
    const int port = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(port > 0);
    Require(sceAudioOutClose(port) == 0);

    AudioOutPortState state;
    const int closedRes = sceAudioOutGetPortState(port, &state);
    Require(closedRes < 0);
}

} // namespace

int main() {
    TestPortStateValidation();
    TestPortStateTypes();
    TestPortStateClosedHandle();

    LibcRunShutdown_nid_postfix();
    return 0;
}
