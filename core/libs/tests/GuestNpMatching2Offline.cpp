#include "prx/libSceNpMatching2/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>





namespace {

constexpr int ResolverTimedOut = static_cast<int>(0x804101E2u);
constexpr int AlreadyInitialized = static_cast<int>(0x80550C02u);
constexpr int NotInitialized = static_cast<int>(0x80550C03u);
constexpr int ContextMax = static_cast<int>(0x80550C04u);
constexpr int ContextNotStarted = static_cast<int>(0x80550C08u);
constexpr int InvalidArgument = static_cast<int>(0x80550C0Au);
constexpr int InvalidContextId = static_cast<int>(0x80550C0Bu);
constexpr std::int32_t ConnStatusInactive = 0;
constexpr std::int32_t StatusSentinel = 0x5A5A5A5A;
constexpr std::uint32_t PeerAddrSentinel = 0xA5A5A5A5u;
constexpr std::uint16_t PeerPortSentinel = 0xA5A5;

int failures = 0;

void Require(bool value, const char* name, const char* what) {
    if (value) return;
    std::fprintf(stderr, "%s: %s\n", name, what);
    ++failures;
}

bool NullStatusThrows(std::uint16_t ctxId) {
    try {
        sceNpMatching2SignalingGetConnectionStatus(ctxId, 1, 1, nullptr, nullptr, nullptr);
    } catch (const std::runtime_error& error) {
        return std::string(error.what()).find("not implemented") != std::string::npos;
    }
    return false;
}

}

int main() {
    NpMatching2InitializeParameter init{};
    NpMatching2CreateContextParameter create{};
    create.size = 0x28;
    unsigned char requestOpt[24] = {};
    std::uint16_t context = 77;

    Require(sceNpMatching2CreateContext(nullptr, nullptr) == NotInitialized, "sceNpMatching2CreateContext", "a call before initialization did not return NOT_INITIALIZED first");
    Require(sceNpMatching2DestroyContext(1) == NotInitialized, "sceNpMatching2DestroyContext", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2ContextStart(1, 0) == NotInitialized, "sceNpMatching2ContextStart", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2ContextStop(1) == NotInitialized, "sceNpMatching2ContextStop", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2AbortContextStart(1) == NotInitialized, "sceNpMatching2AbortContextStart", "a call before initialization did not return NOT_INITIALIZED");
    std::int32_t status = StatusSentinel;
    Require(sceNpMatching2SignalingGetConnectionStatus(1, 1, 1, &status, nullptr, nullptr) == NotInitialized && status == StatusSentinel, "sceNpMatching2SignalingGetConnectionStatus", "a call before initialization did not return NOT_INITIALIZED without writing the status");
    Require(sceNpMatching2SignalingGetConnectionStatus(1, 1, 1, nullptr, nullptr, nullptr) == NotInitialized, "sceNpMatching2SignalingGetConnectionStatus", "a null status before initialization did not return NOT_INITIALIZED first");
    Require(sceNpMatching2RegisterContextCallback(nullptr, nullptr) == NotInitialized, "sceNpMatching2RegisterContextCallback", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2RegisterRoomEventCallback(1, nullptr, nullptr) == NotInitialized, "sceNpMatching2RegisterRoomEventCallback", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2RegisterRoomMessageCallback(1, nullptr, nullptr) == NotInitialized, "sceNpMatching2RegisterRoomMessageCallback", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2RegisterSignalingCallback(1, nullptr, nullptr) == NotInitialized, "sceNpMatching2RegisterSignalingCallback", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2SetDefaultRequestOptParam(1, nullptr) == NotInitialized, "sceNpMatching2SetDefaultRequestOptParam", "a call before initialization did not return NOT_INITIALIZED first");
    Require(sceNpMatching2Terminate() == NotInitialized, "sceNpMatching2Terminate", "terminating before initialization did not return NOT_INITIALIZED");
    Require(sceNpMatching2Initialize(nullptr) == InvalidArgument, "sceNpMatching2Initialize", "a null parameter did not return INVALID_ARGUMENT");
    init.size = 0x10;
    Require(sceNpMatching2Initialize(&init) == InvalidArgument, "sceNpMatching2Initialize", "an invalid parameter size did not return INVALID_ARGUMENT");

    init.size = 0x30;
    Require(sceNpMatching2Initialize(&init) == 0, "sceNpMatching2Initialize", "initialization failed");
    Require(sceNpMatching2Initialize(&init) == AlreadyInitialized, "sceNpMatching2Initialize", "initializing when already initialized did not return ALREADY_INITIALIZED");

    Require(sceNpMatching2CreateContext(nullptr, &context) == InvalidArgument, "sceNpMatching2CreateContext", "a null parameter was taken");
    create.size = 0x30;
    Require(sceNpMatching2CreateContext(&create, &context) == InvalidArgument, "sceNpMatching2CreateContext", "parameter size 0x30 was taken");
    create.size = 0x28;
    Require(sceNpMatching2CreateContext(&create, nullptr) == InvalidArgument, "sceNpMatching2CreateContext", "a null output was taken");
    Require(context == 77, "sceNpMatching2CreateContext", "a refused call wrote the context id");
    Require(sceNpMatching2CreateContext(&create, &context) == 0 && context == 1, "sceNpMatching2CreateContext", "the first context is not id 1");
    Require(sceNpMatching2DestroyContext(1) == 0, "sceNpMatching2DestroyContext", "destroying context 1 failed");
    std::uint16_t created = 0;
    for (std::uint16_t expected = 2; expected <= 10; ++expected) {
        Require(sceNpMatching2CreateContext(&create, &created) == 0 && created == expected, "sceNpMatching2CreateContext", "a context id was reused before the ids up to 10 were handed out");
    }
    Require(sceNpMatching2CreateContext(&create, &created) == 0 && created == 1, "sceNpMatching2CreateContext", "after id 10 the allocation did not wrap to the free id 1");
    created = 77;
    Require(sceNpMatching2CreateContext(&create, &created) == ContextMax && created == 77, "sceNpMatching2CreateContext", "an eleventh context did not return CONTEXT_MAX");
    Require(sceNpMatching2DestroyContext(11) == InvalidContextId, "sceNpMatching2DestroyContext", "an unknown context was destroyed");

    Require(sceNpMatching2ContextStart(1, 0) == ResolverTimedOut, "sceNpMatching2ContextStart", "starting a context did not return the offline resolver timeout");
    Require(sceNpMatching2ContextStart(99, 0) == ResolverTimedOut, "sceNpMatching2ContextStart", "the offline answer did not come before the context check");
    Require(sceNpMatching2ContextStop(1) == ContextNotStarted, "sceNpMatching2ContextStop", "a context that never started did not return CONTEXT_NOT_STARTED");
    Require(sceNpMatching2ContextStop(99) == InvalidContextId, "sceNpMatching2ContextStop", "an unknown context was taken");

    Require(sceNpMatching2AbortContextStart(1) == 0, "sceNpMatching2AbortContextStart", "aborting the start of a context failed");
    Require(sceNpMatching2AbortContextStart(99) == 0, "sceNpMatching2AbortContextStart", "an unknown context id was refused");
    Require(sceNpMatching2ContextStop(1) == ContextNotStarted, "sceNpMatching2AbortContextStart", "the abort changed the start state of the context");

    std::uint32_t peerAddr = PeerAddrSentinel;
    std::uint16_t peerPort = PeerPortSentinel;
    status = StatusSentinel;
    Require(sceNpMatching2SignalingGetConnectionStatus(1, 0x1122334455667788ull, 3, &status, &peerAddr, &peerPort) == 0 && status == ConnStatusInactive, "sceNpMatching2SignalingGetConnectionStatus", "a member of a room that was never joined is not reported inactive");
    Require(peerAddr == PeerAddrSentinel && peerPort == PeerPortSentinel, "sceNpMatching2SignalingGetConnectionStatus", "the peer address or port was written for an inactive member");
    status = StatusSentinel;
    Require(sceNpMatching2SignalingGetConnectionStatus(1, 0, 0, &status, nullptr, nullptr) == 0 && status == ConnStatusInactive, "sceNpMatching2SignalingGetConnectionStatus", "null peer address and port outputs were refused");
    status = StatusSentinel;
    Require(sceNpMatching2SignalingGetConnectionStatus(99, 1, 1, &status, &peerAddr, &peerPort) == InvalidContextId && status == StatusSentinel, "sceNpMatching2SignalingGetConnectionStatus", "an unknown context was taken or its status written");
    Require(NullStatusThrows(1), "sceNpMatching2SignalingGetConnectionStatus", "a null status for a known context did not throw");
    Require(NullStatusThrows(99), "sceNpMatching2SignalingGetConnectionStatus", "a null status for an unknown context did not throw before the context check");
    Require(sceNpMatching2DestroyContext(2) == 0, "sceNpMatching2DestroyContext", "destroying context 2 failed");
    status = StatusSentinel;
    Require(sceNpMatching2SignalingGetConnectionStatus(2, 1, 1, &status, nullptr, nullptr) == InvalidContextId && status == StatusSentinel, "sceNpMatching2SignalingGetConnectionStatus", "a destroyed context was taken");

    Require(sceNpMatching2RegisterContextCallback(nullptr, nullptr) == 0, "sceNpMatching2RegisterContextCallback", "registering the context callback failed");
    Require(sceNpMatching2RegisterRoomEventCallback(99, nullptr, nullptr) == InvalidContextId, "sceNpMatching2RegisterRoomEventCallback", "an unknown context was taken");
    Require(sceNpMatching2RegisterRoomEventCallback(1, nullptr, nullptr) == 0, "sceNpMatching2RegisterRoomEventCallback", "registering failed");
    Require(sceNpMatching2RegisterRoomMessageCallback(99, nullptr, nullptr) == InvalidContextId, "sceNpMatching2RegisterRoomMessageCallback", "an unknown context was taken");
    Require(sceNpMatching2RegisterRoomMessageCallback(1, nullptr, nullptr) == 0, "sceNpMatching2RegisterRoomMessageCallback", "registering failed");
    Require(sceNpMatching2RegisterSignalingCallback(99, nullptr, nullptr) == InvalidContextId, "sceNpMatching2RegisterSignalingCallback", "an unknown context was taken");
    Require(sceNpMatching2RegisterSignalingCallback(1, nullptr, nullptr) == 0, "sceNpMatching2RegisterSignalingCallback", "registering failed");
    Require(sceNpMatching2SetDefaultRequestOptParam(99, nullptr) == InvalidArgument, "sceNpMatching2SetDefaultRequestOptParam", "a null option was not refused before the context");
    Require(sceNpMatching2SetDefaultRequestOptParam(99, requestOpt) == InvalidContextId, "sceNpMatching2SetDefaultRequestOptParam", "an unknown context was taken");
    Require(sceNpMatching2SetDefaultRequestOptParam(1, requestOpt) == 0, "sceNpMatching2SetDefaultRequestOptParam", "setting the default option failed");

    Require(sceNpMatching2Terminate() == 0, "sceNpMatching2Terminate", "termination failed");
    Require(sceNpMatching2Terminate() == NotInitialized, "sceNpMatching2Terminate", "terminating after termination did not return NOT_INITIALIZED");
    Require(sceNpMatching2DestroyContext(1) == NotInitialized, "sceNpMatching2DestroyContext", "a context was destroyed after termination");
    Require(sceNpMatching2AbortContextStart(1) == NotInitialized, "sceNpMatching2AbortContextStart", "a call after termination did not return NOT_INITIALIZED");
    status = StatusSentinel;
    Require(sceNpMatching2SignalingGetConnectionStatus(1, 1, 1, &status, nullptr, nullptr) == NotInitialized && status == StatusSentinel, "sceNpMatching2SignalingGetConnectionStatus", "a call after termination did not return NOT_INITIALIZED without writing the status");
    Require(sceNpMatching2Initialize(&init) == 0, "sceNpMatching2Initialize", "initializing again failed");
    Require(sceNpMatching2RegisterRoomEventCallback(5, nullptr, nullptr) == InvalidContextId, "sceNpMatching2Terminate", "a context created before termination is still found");
    Require(sceNpMatching2CreateContext(&create, &created) == 0 && created == 1, "sceNpMatching2Terminate", "termination did not restart the context ids at 1");
    Require(sceNpMatching2Terminate() == 0, "sceNpMatching2Terminate", "the second termination failed");
    Require(sceNpMatching2Terminate() == NotInitialized, "sceNpMatching2Terminate", "the third termination did not return NOT_INITIALIZED");

    if (failures != 0) return 1;
    std::puts("NpMatching2 offline tests passed");
    return 0;
}
