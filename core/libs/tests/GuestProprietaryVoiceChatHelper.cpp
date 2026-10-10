#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

extern "C" {
int APS5_VABI sceProprietaryVoiceChatHelperInitialize();
int APS5_VABI sceProprietaryVoiceChatHelperTerminate();
int APS5_VABI sceProprietaryVoiceChatHelperSetVoiceChatState();
int APS5_VABI sceProprietaryVoiceChatHelperGetVoiceChatUsageState();
}

static void Require(bool value, const char* what) {
    if (value) return;
    std::printf("ProprietaryVoiceChatHelper: %s\n", what);
    std::abort();
}

template <typename TError, typename TCall>
static bool Throws(TCall call) {
    try {
        call();
    } catch (const TError&) {
        return true;
    }
    return false;
}

int main() {
    Require(Throws<std::logic_error>([] { sceProprietaryVoiceChatHelperSetVoiceChatState(); }),
            "setting the state before initialization did not throw");
    Require(Throws<std::logic_error>([] { sceProprietaryVoiceChatHelperGetVoiceChatUsageState(); }),
            "reading the usage state before initialization did not throw");
    Require(Throws<std::logic_error>([] { sceProprietaryVoiceChatHelperTerminate(); }),
            "terminating before initialization did not throw");

    Require(sceProprietaryVoiceChatHelperInitialize() == 0, "initialization failed");
    Require(Throws<std::logic_error>([] { sceProprietaryVoiceChatHelperInitialize(); }),
            "initializing twice did not throw");
    Require(sceProprietaryVoiceChatHelperSetVoiceChatState() == 0, "setting the state failed");
    Require(Throws<std::runtime_error>([] { sceProprietaryVoiceChatHelperGetVoiceChatUsageState(); }),
            "reading the unknown usage state did not throw");
    Require(sceProprietaryVoiceChatHelperTerminate() == 0, "termination failed");

    Require(Throws<std::logic_error>([] { sceProprietaryVoiceChatHelperSetVoiceChatState(); }),
            "setting the state after termination did not throw");
    Require(sceProprietaryVoiceChatHelperInitialize() == 0, "initialization after termination failed");
    Require(sceProprietaryVoiceChatHelperTerminate() == 0, "termination after reinitialization failed");

    std::puts("ProprietaryVoiceChatHelper tests passed");
    return 0;
}
