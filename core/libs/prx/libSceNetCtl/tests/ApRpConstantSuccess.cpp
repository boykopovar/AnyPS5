#include "prx/libSceNetCtl/libSceNetCtl.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "NetCtl ApRp constant-success check failed at line %u\n", location.line());
    std::abort();
}

template <typename Call>
static void RequireThrows(Call&& call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return;
    }
    std::abort();
}

static void CheckClearEventStubsThrow() {
    for (const int event : {-1, 0, 1, 0x7fffffff}) {
        RequireThrows([=] { sceNetCtlClearEventIpcInt(event); });
        RequireThrows([=] { sceNetCtlClearEventForNpToolkit(event); });
        RequireThrows([=] { sceNetCtlClearEventForLibIpcInt(event); });
    }
}

static void CheckEveryExportAnswersSuccessBeforeInitialization() {
    Require(sceNetCtlApRpCheckCallback() == 0);
    Require(sceNetCtlApRpClearEvent() == 0);
    Require(sceNetCtlApRpGetResult() == 0);
    Require(sceNetCtlApRpGetState() == 0);
    Require(sceNetCtlApRpGetInfo() == 0);
    Require(sceNetCtlApRpRegisterCallback() == 0);
    Require(sceNetCtlApRpUnregisterCallback() == 0);
    Require(sceNetCtlApRpStart() == 0);
    Require(sceNetCtlApRpStartWithRetry() == 0);
    Require(sceNetCtlApRpStartConf() == 0);
    Require(sceNetCtlApRpStop() == 0);
}

static void CheckLifecycleKeepsAnsweringSuccess() {
    Require(sceNetCtlApRpStart() == 0);
    Require(sceNetCtlApRpStartConf() == 0);
    Require(sceNetCtlApRpStartWithRetry() == 0);
    Require(sceNetCtlApRpCheckCallback() == 0);
    Require(sceNetCtlApRpGetState() == 0);
    Require(sceNetCtlApRpGetInfo() == 0);
    Require(sceNetCtlApRpGetResult() == 0);
    Require(sceNetCtlApRpClearEvent() == 0);
    Require(sceNetCtlApRpStop() == 0);
}

static void CheckRepeatedAndUnpairedCallsKeepAnsweringSuccess() {
    Require(sceNetCtlApRpStop() == 0);
    Require(sceNetCtlApRpStop() == 0);
    Require(sceNetCtlApRpClearEvent() == 0);
    Require(sceNetCtlApRpClearEvent() == 0);
    Require(sceNetCtlApRpRegisterCallback() == 0);
    Require(sceNetCtlApRpRegisterCallback() == 0);
    Require(sceNetCtlApRpUnregisterCallback() == 0);
    Require(sceNetCtlApRpUnregisterCallback() == 0);
    Require(sceNetCtlApRpCheckCallback() == 0);
}

int main() {
    CheckClearEventStubsThrow();
    CheckEveryExportAnswersSuccessBeforeInitialization();
    CheckLifecycleKeepsAnsweringSuccess();
    CheckEveryExportAnswersSuccessBeforeInitialization();
    CheckRepeatedAndUnpairedCallsKeepAnsweringSuccess();
    return 0;
}
