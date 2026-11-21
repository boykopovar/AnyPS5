#include "prx/libSceWebRtc/libSceWebRtc.hpp"
#include "prx/libSceSysmodule/include/Api.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int kSysmoduleUnloaded = static_cast<int>(0x805A1001u);
constexpr std::uint16_t kRandomModule = 0x00bau;
constexpr std::uint32_t kInternalModules[] = {
    0x80000020u,
    0x80000069u,
    0x80000043u,
    0x80000068u,
    0x80000092u,
};
constexpr std::uint32_t kResourceArbitratorModule = 0x80000092u;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "GuestWebRtc: %s\n", message);
    std::abort();
}

void RequireAllUnloaded() {
    Require(sceSysmoduleIsLoaded(kRandomModule) == kSysmoduleUnloaded, "Random module should be unloaded");
    for (const std::uint32_t id : kInternalModules) {
        Require(sceSysmoduleIsLoadedInternal(id) == kSysmoduleUnloaded, "internal module should be unloaded");
    }
}

} // namespace

int main() {
    RequireAllUnloaded();

    errno = 71;
    Require(_ZN3sce2np3rtc9TerminateEv() == kSysmoduleUnloaded, "Terminate with nothing loaded must return UNLOADED");
    Require(errno == 71, "Terminate must preserve errno");
    RequireAllUnloaded();

    Require(sceSysmoduleLoadModule(kRandomModule) == 0, "load Random");
    for (const std::uint32_t id : kInternalModules) {
        if (id != kResourceArbitratorModule) {
            Require(sceSysmoduleLoadModuleInternal(id) == 0, "load internal module");
        }
    }
    errno = 73;
    Require(_ZN3sce2np3rtc9TerminateEv() == kSysmoduleUnloaded,
            "Terminate must return ResourceArbitrator result when only the first five modules were loaded");
    Require(errno == 73, "Terminate must preserve errno when unloading the first five modules");
    RequireAllUnloaded();

    Require(sceSysmoduleLoadModuleInternal(kResourceArbitratorModule) == 0, "load ResourceArbitrator alone");
    Require(_ZN3sce2np3rtc9TerminateEv() == 0, "Terminate must return 0 when ResourceArbitrator was loaded");
    RequireAllUnloaded();

    Require(sceSysmoduleLoadModule(kRandomModule) == 0, "load Random first time");
    Require(sceSysmoduleLoadModule(kRandomModule) == 0, "load Random second time");
    for (const std::uint32_t id : kInternalModules) {
        Require(sceSysmoduleLoadModuleInternal(id) == 0, "load internal module first time");
        Require(sceSysmoduleLoadModuleInternal(id) == 0, "load internal module second time");
    }
    errno = 79;
    Require(_ZN3sce2np3rtc9TerminateEv() == 0, "first Terminate on double-loaded modules must return 0");
    Require(errno == 79, "first Terminate must preserve errno");
    Require(sceSysmoduleIsLoaded(kRandomModule) == 0, "Random must still be loaded after one Terminate");
    for (const std::uint32_t id : kInternalModules) {
        Require(sceSysmoduleIsLoadedInternal(id) == 0, "internal module must still be loaded after one Terminate");
    }
    Require(_ZN3sce2np3rtc9TerminateEv() == 0, "second Terminate on double-loaded modules must return 0");
    RequireAllUnloaded();
    Require(_ZN3sce2np3rtc9TerminateEv() == kSysmoduleUnloaded, "third Terminate must return UNLOADED");

    std::puts("GuestWebRtc tests passed");
    return 0;
}
