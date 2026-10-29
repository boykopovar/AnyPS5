#include "prx/libSceNetCtl/libSceNetCtl.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <thread>


static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "NetCtl IPv6 callback check failed at line %u\n", location.line());
    std::abort();
}

struct Probe {
    int calls = 0;
    int event = 0;
    std::thread::id thread;
};

static void APS5_VABI Record(int event, void* argument) {
    auto& probe = *static_cast<Probe*>(argument);
    ++probe.calls;
    probe.event = event;
    probe.thread = std::this_thread::get_id();
}

static int ExpectedEvent(bool ipv6) {
    int state = -1;
    Require((ipv6 ? sceNetCtlGetStateV6(&state) : sceNetCtlGetState(&state)) == 0);
    return state == 3 ? 3 : 1;
}

struct RegisterDuringCheck {
    Probe current;
    Probe later;
    int laterId = -1;
};

static void APS5_VABI RegisterV6(int event, void* argument) {
    auto& request = *static_cast<RegisterDuringCheck*>(argument);
    Record(event, &request.current);
    Require(sceNetCtlRegisterCallbackV6(Record, &request.later, &request.laterId) == 0);
}

struct Reentrant {
    Probe probe;
    int id = -1;
    int unregisterResult = -1;
    int checkResult = -1;
};

static void APS5_VABI UnregisterAndReenter(int event, void* argument) {
    auto& request = *static_cast<Reentrant*>(argument);
    Record(event, &request.probe);
    request.unregisterResult = sceNetCtlUnregisterCallbackV6(request.id);
    request.checkResult = sceNetCtlCheckCallback();
}

int main() {
    Require(sceNetCtlCheckCallback() == 0);
    const auto checkingThread = std::this_thread::get_id();
    const int ipv6Event = ExpectedEvent(true);
    Probe v6;
    int v6Id = -1;
    Require(sceNetCtlRegisterCallbackV6(Record, &v6, &v6Id) == 0);
    Require(v6.calls == 0);
    Require(sceNetCtlCheckCallback() == 0);
    Require(v6.calls == 1 && v6.event == ipv6Event && v6.thread == checkingThread);
    Require(sceNetCtlCheckCallback() == 0 && v6.calls == 1);
    Require(sceNetCtlUnregisterCallbackV6(v6Id) == 0);
    Require(sceNetCtlCheckCallback() == 0 && v6.calls == 1);

    RegisterDuringCheck registration;
    int v4Id = -1;
    Require(sceNetCtlRegisterCallback(RegisterV6, &registration, &v4Id) == 0);
    Require(sceNetCtlRegisterCallbackV6(Record, &v6, &v6Id) == 0);
    Require(sceNetCtlCheckCallback() == 0);
    Require(registration.current.calls == 1 && registration.current.event == ExpectedEvent(false));
    Require(registration.later.calls == 0 && v6.calls == 2);
    Require(sceNetCtlCheckCallback() == 0);
    Require(registration.later.calls == 1 && registration.later.event == ipv6Event && registration.later.thread == checkingThread);
    Require(v6.calls == 2 && registration.current.calls == 1);
    Require(sceNetCtlUnregisterCallback(v4Id) == 0);
    Require(sceNetCtlUnregisterCallbackV6(v6Id) == 0);
    Require(sceNetCtlUnregisterCallbackV6(registration.laterId) == 0);

    Reentrant reentrant;
    Probe other;
    Require(sceNetCtlRegisterCallbackV6(UnregisterAndReenter, &reentrant, &reentrant.id) == 0);
    Require(sceNetCtlRegisterCallbackV6(Record, &other, &v6Id) == 0);
    Require(sceNetCtlCheckCallback() == 0);
    Require(reentrant.probe.calls == 1 && reentrant.probe.event == ipv6Event);
    Require(reentrant.unregisterResult == 0 && reentrant.checkResult == 0);
    Require(other.calls == 1 && other.event == ipv6Event);
    Require(sceNetCtlCheckCallback() == 0 && other.calls == 1);
    Require(sceNetCtlUnregisterCallbackV6(v6Id) == 0);

    std::array<Probe, 8> probes{};
    std::array<int, 8> ids{};
    std::array<Probe, 8> ipv4Probes{};
    std::array<int, 8> ipv4Ids{};
    for (std::size_t index = 0; index < probes.size(); ++index) {
        Require(sceNetCtlRegisterCallbackV6(Record, &probes[index], &ids[index]) == 0);
        Require(sceNetCtlRegisterCallback(Record, &ipv4Probes[index], &ipv4Ids[index]) == 0);
    }
    Require(sceNetCtlCheckCallback() == 0);
    for (std::size_t index = 0; index < probes.size(); ++index) {
        Require(probes[index].calls == 1 && probes[index].event == ipv6Event && probes[index].thread == checkingThread);
        Require(ipv4Probes[index].calls == 1 && ipv4Probes[index].event == ExpectedEvent(false) && ipv4Probes[index].thread == checkingThread);
        Require(sceNetCtlUnregisterCallbackV6(ids[index]) == 0);
        Require(sceNetCtlUnregisterCallback(ipv4Ids[index]) == 0);
    }
    Probe toolkit, isolatedV6;
    int toolkitId = -1;
    Require(sceNetCtlRegisterCallbackForNpToolkit(Record, &toolkit, &toolkitId) == 0);
    Require(sceNetCtlRegisterCallbackV6(Record, &isolatedV6, &v6Id) == 0);
    Require(sceNetCtlCheckCallbackForNpToolkit() == 0);
    Require(toolkit.calls == 1 && toolkit.event == ExpectedEvent(false) && isolatedV6.calls == 0);
    Require(sceNetCtlCheckCallback() == 0);
    Require(toolkit.calls == 1 && isolatedV6.calls == 1 && isolatedV6.event == ipv6Event);
    Require(sceNetCtlCheckCallbackForNpToolkit() == 0 && toolkit.calls == 1);
    Require(sceNetCtlUnregisterCallbackForNpToolkit(toolkitId) == 0);
    Require(sceNetCtlUnregisterCallbackV6(v6Id) == 0);

}
