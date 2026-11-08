#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

void TestAudioOutGetSystemState() {
    Require(sceAudioOutInit() == 0 || sceAudioOutInit() == static_cast<int>(0x8026000e));

    const int nullRes = sceAudioOutGetSystemState(nullptr);
    Require(nullRes < 0);

    AudioOutSystemState state;
    Require(sceAudioOutGetSystemState(&state) == 0);
}

} // namespace

int main() {
    TestAudioOutGetSystemState();

    LibcRunShutdown_nid_postfix();
    return 0;
}
