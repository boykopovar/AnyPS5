#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <stdexcept>

extern "C" {
int APS5_VABI sceAudioOut2Initialize();
int APS5_VABI sceAudioOut2Set3DLatency(int, std::uint32_t);
int APS5_VABI sceAudioOut2MasteringInit(std::uint32_t);
int APS5_VABI sceAudioOut2MasteringTerm();
int APS5_VABI sceAudioOut2MasteringSetParam(const void*, std::uint32_t, std::uint32_t);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr int systemUser = 0xFF;
constexpr int user = 0x10000000;

void Initialize() {
    RequireEqual(sceAudioOut2Initialize(), 0, "initialization must succeed");
}

class Mastering {
public:
    Mastering() {
        RequireEqual(sceAudioOut2MasteringInit(0), 0, "flags 0 must be accepted");
        initialized = true;
    }

    ~Mastering() {
        if (initialized) sceAudioOut2MasteringTerm();
    }

    Mastering(const Mastering&) = delete;
    Mastering& operator=(const Mastering&) = delete;

    void Term() {
        initialized = false;
        RequireEqual(sceAudioOut2MasteringTerm(), 0, "termination must be accepted");
    }

private:
    bool initialized = false;
};

const Case latencyAccepted{"Set3DLatency_SystemUserOneOrTwo_Succeeds", [] {
    Initialize();
    RequireEqual(sceAudioOut2Set3DLatency(systemUser, 2), 0, "latency 2 for the system user must be accepted");
    RequireEqual(sceAudioOut2Set3DLatency(systemUser, 2), 0, "latency 2 must be accepted again");
    RequireEqual(sceAudioOut2Set3DLatency(systemUser, 1), 0, "latency 1 for the system user must be accepted");
}};

const Case latencyRejected{"Set3DLatency_OtherLatencyOrUser_Throws", [] {
    Initialize();
    RequireThrows<std::runtime_error>([] { sceAudioOut2Set3DLatency(systemUser, 0); }, "latency 0 must throw");
    RequireThrows<std::runtime_error>([] { sceAudioOut2Set3DLatency(systemUser, 3); }, "latency 3 must throw");
    RequireThrows<std::runtime_error>([] { sceAudioOut2Set3DLatency(user, 2); }, "a user other than the system user must throw");
}};

const Case masteringFlags{"MasteringInit_NonZeroFlags_Throws", [] {
    Initialize();
    RequireThrows<std::runtime_error>([] { sceAudioOut2MasteringInit(1); }, "flags 1 must throw");
}};

const Case masteringTerm{"MasteringTerm_TwoInitializations_TerminatesEach", [] {
    Initialize();
    Mastering first;
    Mastering second;
    second.Term();
    first.Term();
}};

const Case masteringParam{"MasteringSetParam_ParamsOrNull_AcceptsOrThrows", [] {
    Initialize();
    const std::uint32_t params[4] = {1u, 0u, 0u, 0u};
    RequireEqual(sceAudioOut2MasteringSetParam(params, 0, 0), 0, "mastering parameters must be accepted");
    RequireThrows<std::runtime_error>([] { sceAudioOut2MasteringSetParam(nullptr, 0, 0); }, "null mastering parameters must throw");
}};

} // namespace
