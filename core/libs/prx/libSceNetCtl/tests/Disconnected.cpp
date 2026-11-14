#include "prx/libSceNetCtl/libSceNetCtl.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "NetCtl disconnected check failed at line %u\n", location.line());
    std::abort();
}

static void CheckState() {
    int state = -1;
    Require(sceNetCtlGetState(&state) == 0 && state == 0);
    int stateV6 = -1;
    Require(sceNetCtlGetStateV6(&stateV6) == 0 && stateV6 == 0);
    int stateIpc = -1;
    Require(sceNetCtlGetStateIpcInt(&stateIpc) == 0 && stateIpc == 0);
    int stateV6Ipc = -1;
    Require(sceNetCtlGetStateV6IpcInt(&stateV6Ipc) == 0 && stateV6Ipc == 0);
}

static void CheckQueries() {
    constexpr int kNotConnected = static_cast<int>(0x80412108);
    NetCtlIfStat stat{};
    Require(sceNetCtlGetIfStat(&stat) == kNotConnected);

    NetCtlInfo info{};
    Require(sceNetCtlGetInfo(1, &info) == kNotConnected);

    NetCtlNatInfo nat{};
    nat.size = sizeof(NetCtlNatInfo);
    Require(sceNetCtlGetNatInfo(&nat) == kNotConnected);
}

struct CallbackProbe {
    int calls = 0;
    int event = 0;
};

static void APS5_VABI RecordCallback(int event, void* arg) {
    auto& probe = *static_cast<CallbackProbe*>(arg);
    ++probe.calls;
    probe.event = event;
}

static void CheckCallback() {
    CallbackProbe probe{};
    int cid = -1;
    Require(sceNetCtlRegisterCallback(RecordCallback, &probe, &cid) == 0 && cid >= 0);
    Require(sceNetCtlCheckCallback() == 0);
    Require(probe.calls == 1 && probe.event == 1);
    Require(sceNetCtlUnregisterCallback(cid) == 0);
}

int main() {
    Require(sceNetCtlInit() == 0);
    int initial = -1;
    Require(sceNetCtlGetState(&initial) == 0);
    if (initial == 0) {
        std::puts("skipped, the host has no IPv4 address");
        return 77;
    }
#ifdef _WIN32
    _putenv("APS5_NET_DISCONNECTED=1");
#else
    setenv("APS5_NET_DISCONNECTED", "1", 1);
#endif
    CheckState();
    CheckQueries();
    CheckCallback();
    return 0;
}
