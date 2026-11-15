#include "prx/libSceIpmi/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libSceIpmi config estimator: %s\n", message);
    std::abort();
}

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
}

template <typename Fn>
void RequireThrows(Fn&& fn, const char* message) {
    bool threw = false;
    try {
        fn();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, message);
}

}

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);

    alignas(8) unsigned char config[0x40];
    std::memset(config, 0, sizeof(config));

    Require(_ZN4IPMI7Session6Config25estimateSessionMemorySizeEv(config) == 0, "Session::Config size is zero");
    Require(_ZN4IPMI6Client6Config24estimateClientMemorySizeEv(config) == 0x1000, "Client::Config size is 0x1000");

    const std::uint64_t first[] = {0x200, 0x300};
    errno = 71;
    Require(_ZNK4IPMI6Server6Config29estimateTempWorkingMemorySizeEv(first) == 0x400 && errno == 71,
            "Server::Config takes the larger first value");
    const std::uint64_t second[] = {0x500, 0x100};
    Require(_ZNK4IPMI6Server6Config29estimateTempWorkingMemorySizeEv(second) == 0x600, "Server::Config takes the larger second value");

    for (const void* badPtr : {static_cast<const void*>(nullptr), reinterpret_cast<const void*>(static_cast<std::uintptr_t>(0x10))}) {
        errno = 71;
        RequireThrows([&] { _ZNK4IPMI6Server6Config29estimateTempWorkingMemorySizeEv(badPtr); }, "unreadable Server::Config estimator");
        Require(errno == 71, "Server::Config estimator changed errno");
    }
    for (void* badPtr : {static_cast<void*>(nullptr), reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x10))}) {
        errno = 71;
        RequireThrows([&] { _ZN4IPMI6Server6ConfigC1Ev(badPtr); }, "unwritable Server::Config::C1");
        Require(errno == 71, "Server::Config::C1 changed errno");
        errno = 71;
        RequireThrows([&] { _ZN4IPMI6Client6ConfigC1Ev(badPtr); }, "unwritable Client::Config::C1");
        Require(errno == 71, "Client::Config::C1 changed errno");
        errno = 71;
        RequireThrows([&] { _ZN4IPMI6Client6Config26SyncMethodInvocationConfigC1Ev(badPtr); }, "unwritable SyncMethodInvocationConfig::C1");
        Require(errno == 71, "SyncMethodInvocationConfig::C1 changed errno");
        errno = 71;
        RequireThrows([&] { _ZN4IPMI6Client6Config14MsgQueueConfigC1Ev(badPtr); }, "unwritable MsgQueueConfig::C1");
        Require(errno == 71, "MsgQueueConfig::C1 changed errno");
    }

    std::array<std::uint8_t, 0x158> clientConfig{};
    clientConfig.fill(0xAA);
    errno = 71;
    _ZN4IPMI6Client6ConfigC1Ev(clientConfig.data());
    Require(errno == 71, "valid Client::Config::C1 changed errno");
    Require(clientConfig[0x00] == 0 && clientConfig[0x01] == 0xAA && clientConfig[0x19] == 0 && clientConfig[0x1a] == 0xAA,
            "Client::Config::C1 wrote outside initialized bytes");
    std::uint32_t invocationFlag = 0;
    std::memcpy(&invocationFlag, clientConfig.data() + 0x20, sizeof(invocationFlag));
    Require(invocationFlag == 1 && clientConfig[0x24] == 0xAA, "Client::Config invocation padding touched");

    std::array<std::uint8_t, 0x40> serverConfig{};
    serverConfig.fill(0xAA);
    errno = 71;
    _ZN4IPMI6Server6ConfigC1Ev(serverConfig.data());
    Require(errno == 71 && serverConfig[0x18] == 0 && serverConfig[0x1a] == 0xAA && serverConfig[0x32] == 0 && serverConfig[0x34] == 0xAA,
            "Server::Config::C1 padding check");

    alignas(8) unsigned char object[0x40];
    std::memset(object, 0x5a, sizeof(object));
    Require(_ZN4IPMI4impl10ClientImpl12tryGetResultEjPiPvPmm(object, 0, nullptr, nullptr, nullptr, 0) == 0,
            "ClientImpl::tryGetResult returns zero");
    Require(_ZN4IPMI4impl10ClientImpl17invokeAsyncMethodEjPKvmPjPKNS_6Client12EventNotifeeE(object, 0, nullptr, 0, nullptr, nullptr) == 0,
            "ClientImpl::invokeAsyncMethod returns zero");
    Require(_ZN4IPMI4impl11SessionImpl11tryDispatchEPvm(object, nullptr, 0) == 0, "SessionImpl::tryDispatch returns zero");
    for (std::size_t index = 0; index < sizeof(object); ++index) {
        Require(object[index] == 0x5a, "the query leaves the object untouched");
    }

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    std::puts("libSceIpmi config estimator checks passed");
    return 0;
}
