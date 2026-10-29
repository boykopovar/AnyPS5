#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

namespace {

constexpr int invalidPointer = static_cast<int>(0x80260004);

void TestGetSystemState() {
    AudioOutSystemState state{};
    state.loudness = -1.0f;
    state.reserved8[0] = 0xAA;
    state.reserved[2] = ~0ull;
    Require(sceAudioOutGetSystemState(&state) == 0, "sceAudioOutGetSystemState must succeed with valid pointer");
    Require(state.loudness == 0.0f, "loudness must be reported as 0.0f");
    Require(state.reserved8[0] == 0 && state.reserved[2] == 0, "reserved fields must be cleared");
}

void TestInvalidPointer() {
    Require(sceAudioOutGetSystemState(nullptr) == invalidPointer, "null pointer must return SCE_AUDIO_OUT_ERROR_INVALID_POINTER");
}

}

int main() {
    TestGetSystemState();
    TestInvalidPointer();
    return 0;
}
