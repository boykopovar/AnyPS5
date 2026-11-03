#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <cstdlib>

static void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

int main() {
    constexpr int invalidPointer = static_cast<int>(0x80260004);
    constexpr int invalidHandle = static_cast<int>(0x80260001);

    Require(sceAudioOutInit() == 0);
    Require(sceAudioOutGetPortState(invalidHandle, nullptr) == invalidPointer);
    Require(sceAudioOutGetPortState(0, nullptr) == invalidPointer);

    LibcRunShutdown_nid_postfix();
    return 0;
}
