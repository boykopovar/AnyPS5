#include "prx/libSceNetCtl/libSceNetCtl.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif


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

class ReadOnlyId {
public:
    ReadOnlyId() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        bytes = info.dwPageSize;
        memory = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        Require(memory != nullptr);
#else
        const long page = sysconf(_SC_PAGESIZE);
        Require(page > 0);
        bytes = static_cast<std::size_t>(page);
        memory = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(memory != MAP_FAILED);
#endif
        *Get() = -123;
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(memory, bytes, PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(memory, bytes, PROT_READ) == 0);
#endif
    }
    ~ReadOnlyId() {
#ifdef _WIN32
        Require(VirtualFree(memory, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(memory, bytes) == 0);
#endif
    }
    ReadOnlyId(const ReadOnlyId&) = delete;
    ReadOnlyId& operator=(const ReadOnlyId&) = delete;
    int* Get() { return static_cast<int*>(memory); }
private:
    void* memory;
    std::size_t bytes;
};

static void RegistrationOutputs() {
    using Register = decltype(&sceNetCtlRegisterCallback);
    using Unregister = decltype(&sceNetCtlUnregisterCallback);
    using Check = decltype(&sceNetCtlCheckCallback);
    struct Api {
        Register registerCallback;
        Unregister unregisterCallback;
        Check checkCallback;
    };
    const std::array<Api, 6> apis{{
        {sceNetCtlRegisterCallback, sceNetCtlUnregisterCallback, sceNetCtlCheckCallback},
        {sceNetCtlRegisterCallbackV6, sceNetCtlUnregisterCallbackV6, sceNetCtlCheckCallback},
        {sceNetCtlRegisterCallbackIpcInt, sceNetCtlUnregisterCallbackIpcInt, sceNetCtlCheckCallback},
        {sceNetCtlRegisterCallbackV6IpcInt, sceNetCtlUnregisterCallbackV6IpcInt, sceNetCtlCheckCallback},
        {sceNetCtlRegisterCallbackForNpToolkit, sceNetCtlUnregisterCallbackForNpToolkit, sceNetCtlCheckCallbackForNpToolkit},
        {sceNetCtlRegisterCallbackForLibIpcInt, sceNetCtlUnregisterCallbackForLibIpcInt, sceNetCtlCheckCallbackForLibIpcInt}
    }};
    constexpr int invalidAddress = static_cast<int>(0x80412107);
    constexpr int idNotFound = static_cast<int>(0x80412104);
    constexpr int callbackMax = static_cast<int>(0x80412103);
    auto* inaccessible = reinterpret_cast<int*>(0x10);
    ReadOnlyId readOnly;
    for (const auto& api : apis) {
        Probe failed;
        errno = EDOM;
        Require(api.registerCallback(nullptr, &failed, inaccessible) == invalidAddress);
        Require(api.registerCallback(Record, &failed, nullptr) == invalidAddress);
        for (int attempt = 0; attempt < 10; ++attempt) {
            Require(api.registerCallback(Record, &failed, inaccessible) == invalidAddress);
            Require(errno == EDOM);
            Require(api.registerCallback(Record, &failed, readOnly.Get()) == invalidAddress);
            Require(errno == EDOM && *readOnly.Get() == -123);
        }
        Require(api.unregisterCallback(0) == idNotFound);
        Require(api.checkCallback() == 0 && failed.calls == 0);
        std::array<Probe, 8> probes{};
        std::array<int, 8> ids{};
        for (std::size_t index = 0; index < ids.size(); ++index) {
            struct Output {
                int before = -1;
                int id = -1;
                int after = -1;
            } output;
            errno = EDOM;
            Require(api.registerCallback(Record, &probes[index], &output.id) == 0);
            Require(errno == EDOM);
            Require(output.id == static_cast<int>(index) && output.before == -1 && output.after == -1);
            ids[index] = output.id;
        }
        int untouched = -1;
        Require(api.registerCallback(Record, &failed, &untouched) == callbackMax && untouched == -1);
        Require(api.registerCallback(Record, &failed, inaccessible) == callbackMax);
        Require(api.registerCallback(Record, &failed, readOnly.Get()) == callbackMax);
        Require(api.registerCallback(nullptr, &failed, &untouched) == invalidAddress && untouched == -1);
        Require(api.checkCallback() == 0 && failed.calls == 0);
        for (std::size_t index = 0; index < ids.size(); ++index) {
            Require(probes[index].calls == 1);
            Require(api.unregisterCallback(ids[index]) == 0);
        }
    }
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
    RegistrationOutputs();
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
