#include "prx/libSceVideoOut/include/KeepAwake.hpp"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

static void Require(bool value, const char* reason) {
    if (!value) {
        std::fprintf(stderr, "%s\n", reason);
        std::abort();
    }
}

namespace {

int disables = 0;
int enables = 0;
bool enabled = true;
bool refuseDisable = false;

void FakeDisable() {
    ++disables;
    if (!refuseDisable) enabled = false;
}

void FakeEnable() {
    ++enables;
    enabled = true;
}

bool FakeIsEnabled() {
    return enabled;
}

const KeepAwakeBackend fakeBackend{FakeDisable, FakeEnable, FakeIsEnabled};

void TestParser() {
    Require(KeepAwakeEnabledFromValue(nullptr), "unset ANYPS5_KEEP_AWAKE did not keep the screen awake");
    Require(!KeepAwakeEnabledFromValue("0"), "ANYPS5_KEEP_AWAKE=0 did not turn keep-awake off");
    for (const char* value : {"", "1", "00", "off", "false", " 0", "0 ", "2", "yes"}) {
        Require(KeepAwakeEnabledFromValue(value), "a value other than 0 did not keep the screen awake");
    }
}

void TestAcquireRelease() {
    HostKeepAwake keepAwake(fakeBackend);
    keepAwake.Release();
    Require(disables == 0 && enables == 0, "releasing an inhibit that was never taken called the backend");
    keepAwake.Acquire();
    keepAwake.Acquire();
    Require(disables == 1, "a second acquire called the backend again");
    keepAwake.Release();
    keepAwake.Release();
    Require(enables == 1, "a second release called the backend again");
    keepAwake.Acquire();
    Require(disables == 2, "acquiring after a release did not disable the screen saver again");
    keepAwake.Release();
    Require(enables == 2, "releasing after a re-acquire did not enable the screen saver");
}

}

void TestRefusedInhibitOnlyLogs() {
    refuseDisable = true;
    HostKeepAwake keepAwake(fakeBackend);
    const int before = enables;
    keepAwake.Acquire();
    Require(disables == 3 && enabled, "the refused inhibit was not attempted");
    keepAwake.Release();
    refuseDisable = false;
    Require(enables == before + 1, "a refused inhibit was not released afterwards");
}

void TestSdlBackendWithoutVideo() {
    const auto& backend = SdlKeepAwakeBackend();
    backend.disable();
    backend.enable();
    Require(backend.isEnabled(), "SDL reported the screen saver suspended with no video subsystem");
}

int main() {
    TestParser();
    TestAcquireRelease();
    TestRefusedInhibitOnlyLogs();
    TestSdlBackendWithoutVideo();
    return 0;
}
