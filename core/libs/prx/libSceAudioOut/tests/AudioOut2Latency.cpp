#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

extern "C" {
int APS5_VABI sceAudioOut2Initialize();
int APS5_VABI sceAudioOut2Set3DLatency(int, std::uint32_t);
int APS5_VABI sceAudioOut2MasteringInit(std::uint32_t);
}

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

template<typename TFunction>
static bool ThrowsRuntimeError(TFunction function) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

namespace {

constexpr int systemUser = 0xFF;
constexpr int user = 0x10000000;

void TestSet3DLatency() {
    Require(sceAudioOut2Set3DLatency(systemUser, 2) == 0, "latency 2 for the system user must be accepted");
    Require(sceAudioOut2Set3DLatency(systemUser, 2) == 0, "latency 2 must be accepted again");
    Require(ThrowsRuntimeError([] { sceAudioOut2Set3DLatency(systemUser, 1); }), "latency 1 must throw");
    Require(ThrowsRuntimeError([] { sceAudioOut2Set3DLatency(systemUser, 3); }), "latency 3 must throw");
    Require(ThrowsRuntimeError([] { sceAudioOut2Set3DLatency(user, 2); }), "a user other than the system user must throw");
}

void TestMasteringInit() {
    Require(sceAudioOut2MasteringInit(0) == 0, "flags 0 must be accepted");
    Require(ThrowsRuntimeError([] { sceAudioOut2MasteringInit(1); }), "flags 1 must throw");
}

}

int main() {
    Require(sceAudioOut2Initialize() == 0, "initialization must succeed");
    TestSet3DLatency();
    TestMasteringInit();
    return 0;
}
