#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" {
int APS5_VABI sceAudioOutGetSystemState(AudioOutSystemState*);
}

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
    Require(sceAudioOutGetSystemState(&state) == 0, "sceAudioOutGetSystemState must succeed with valid pointer");
    Require(state.loudness == 0.0f, "loudness must be reported as 0.0f");
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
