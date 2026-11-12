#include "prx/libSceNpRemotePlaySessionSignaling/libSceNpRemotePlaySessionSignaling.hpp"
#include "prx/libSceNpSessionSignaling/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int InvalidArgument = static_cast<int>(0x80553303u);
constexpr int Unavailable = static_cast<int>(0x80552D06u);
constexpr int NotInitialized = static_cast<int>(0x80553301u);
constexpr int AlreadyInitialized = static_cast<int>(0x80553302u);
constexpr int ContextNotFound = static_cast<int>(0x80553308u);
constexpr std::uint32_t Stale = 0x5A5A5A5Au;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "NpRemotePlaySessionSignaling: %s\n", message);
    std::abort();
}

void CheckBeforeInitialize() {
    Require(sceNpRemotePlaySessionSignalingTerminate() == NotInitialized, "Terminate before Initialize");
    Require(sceNpRemotePlaySessionSignalingGetMemoryInfo(nullptr) == InvalidArgument, "GetMemoryInfo: null output");
    SessionSignalingMemoryInfo memory{Stale, Stale, Stale};
    Require(sceNpRemotePlaySessionSignalingGetMemoryInfo(&memory) == NotInitialized, "GetMemoryInfo before Initialize");
    Require(memory.poolSize == Stale && memory.usedSize == Stale && memory.peakUsedSize == Stale,
        "GetMemoryInfo: a failed call writes nothing");
    Require(sceNpRemotePlaySessionSignalingGetLocalNetInfo(1, nullptr) == InvalidArgument, "GetLocalNetInfo: null output");
    std::int32_t status = static_cast<std::int32_t>(Stale);
    Require(sceNpRemotePlaySessionSignalingGetConnectionStatus(1, 2, nullptr, nullptr, nullptr) == InvalidArgument,
        "GetConnectionStatus: null status");
    Require(sceNpRemotePlaySessionSignalingGetConnectionStatus(1, 2, &status, nullptr, nullptr) == NotInitialized,
        "GetConnectionStatus before Initialize");
    Require(status == static_cast<std::int32_t>(Stale), "GetConnectionStatus: a failed call writes nothing");
    Require(sceNpRemotePlaySessionSignalingInitialize(nullptr) == InvalidArgument, "Initialize: null parameter");
}

void CheckInitialized() {
    SessionSignalingInitParam param{};
    param.userId = 7;
    param.poolSize = 0x40000;
    param.affinityMask = 1;
    param.threadPriority = 700;
    param.stackSize = 0x4000;
    Require(sceNpRemotePlaySessionSignalingInitialize(&param) == 0, "Initialize");
    Require(sceNpRemotePlaySessionSignalingInitialize(&param) == AlreadyInitialized, "Initialize twice");
    Require(sceNpSessionSignalingInitialize(&param) == AlreadyInitialized, "the session signaling library is initialized");

    SessionSignalingMemoryInfo memory{Stale, Stale, Stale};
    errno = 71;
    Require(sceNpRemotePlaySessionSignalingGetMemoryInfo(reinterpret_cast<SessionSignalingMemoryInfo*>(0x10)) == InvalidArgument,
        "GetMemoryInfo: unmapped pointer");
    Require(errno == 71, "GetMemoryInfo: preserves errno");
    Require(sceNpRemotePlaySessionSignalingGetMemoryInfo(&memory) == 0, "GetMemoryInfo");
    Require(memory.poolSize == 0x40000 && memory.usedSize == 0 && memory.peakUsedSize == 0, "GetMemoryInfo: values");

    NpRemotePlaySessionSignalingLocalNetInfo net{Stale, Stale, Stale};
    Require(sceNpRemotePlaySessionSignalingGetLocalNetInfo(1, &net) == Unavailable, "GetLocalNetInfo: library result");
    Require(net.localAddress == Stale && net.mappedAddress == Stale && net.mappedPort == Stale,
        "GetLocalNetInfo: a failed call writes nothing");

    std::int32_t status = static_cast<std::int32_t>(Stale);
    Require(sceNpRemotePlaySessionSignalingGetConnectionStatus(999, 2, &status, nullptr, nullptr) == ContextNotFound,
        "GetConnectionStatus: unknown context");
    Require(status == static_cast<std::int32_t>(Stale), "GetConnectionStatus: a failed call writes nothing");

    std::uint32_t property = Stale;
    Require(sceNpRemotePlaySessionSignalingGetCustomProperty(999, &property) == ContextNotFound, "GetCustomProperty");
    Require(property == Stale, "GetCustomProperty: a failed call writes nothing");
    Require(sceNpRemotePlaySessionSignalingSetCustomProperty(999, nullptr, nullptr) == InvalidArgument, "SetCustomProperty");
    Require(sceNpRemotePlaySessionSignalingLeaveSession(999) == ContextNotFound, "LeaveSession");

    Require(sceNpRemotePlaySessionSignalingTerminate() == 0, "Terminate");
    Require(sceNpRemotePlaySessionSignalingTerminate() == NotInitialized, "Terminate twice");
    Require(sceNpSessionSignalingGetMemoryInfo(&memory) == NotInitialized, "the session signaling library is terminated");
}

}

int main() {
    CheckBeforeInitialize();
    CheckInitialized();
    std::puts("NpRemotePlaySessionSignaling tests passed");
    return 0;
}
