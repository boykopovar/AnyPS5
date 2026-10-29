#include "prx/libSceNpSignaling/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

constexpr int SignedOut = static_cast<int>(0x80550006u);
constexpr int NotInitialized = static_cast<int>(0x80552701u);
constexpr int AlreadyInitialized = static_cast<int>(0x80552702u);
constexpr int CtxIdNotAvailable = static_cast<int>(0x80552704u);
constexpr int CtxNotFound = static_cast<int>(0x80552705u);
constexpr int ReqNotFound = static_cast<int>(0x80552707u);
constexpr int ConnNotFound = static_cast<int>(0x8055270Eu);
constexpr int ResultNotFound = static_cast<int>(0x80552713u);
constexpr int InvalidArgument = static_cast<int>(0x80552715u);
constexpr int OwnNpId = static_cast<int>(0x80552716u);

struct SignalingNpId {
    char handle[16];
    char term;
    char dummy[3];
    std::uint8_t opt[8];
    std::uint8_t reserved[8];
};
static_assert(sizeof(SignalingNpId) == 36);

int failures = 0;

void Require(bool value, const char* name, const char* what) {
    if (value) return;
    std::fprintf(stderr, "%s: %s\n", name, what);
    ++failures;
}

SignalingNpId MakeNpId(const char* handle) {
    SignalingNpId id{};
    std::memcpy(id.handle, handle, std::strlen(handle));
    return id;
}

}

int main() {
    const SignalingNpId owner = MakeNpId("player_1");
    const SignalingNpId peer = MakeNpId("peer-2");
    const SignalingNpId maximum = MakeNpId("abcdefghijklmnop");
    const SignalingNpId tooShort = MakeNpId("ab");
    const SignalingNpId leadingUnderscore = MakeNpId("_player");
    const SignalingNpId withDot = MakeNpId("pl.ayer");
    SignalingNpId latin = MakeNpId("pl\xE9yer");
    SignalingNpId upper = MakeNpId("Player9");
    SignalingNpId unterminated = MakeNpId("player_1");
    unterminated.term = 'x';
    unsigned char info[64];
    std::memset(info, 0xA5, sizeof(info));
    int context = -7;
    int connection = -7;

    Require(sceNpSignalingCreateContext(nullptr, nullptr, nullptr, nullptr) == NotInitialized, "sceNpSignalingCreateContext", "a call before initialization did not return NOT_INITIALIZED first");
    Require(sceNpSignalingDeleteContext(1) == NotInitialized, "sceNpSignalingDeleteContext", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpSignalingSetContextOption(1, 1, 1) == NotInitialized, "sceNpSignalingSetContextOption", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpSignalingActivateConnection(1, nullptr, nullptr) == NotInitialized, "sceNpSignalingActivateConnection", "a call before initialization did not return NOT_INITIALIZED first");
    Require(sceNpSignalingTerminateConnection(1, 1) == NotInitialized, "sceNpSignalingTerminateConnection", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpSignalingDeactivateConnection(1, 1) == NotInitialized, "sceNpSignalingDeactivateConnection", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpSignalingGetConnectionInfo(1, 1, 1, nullptr) == NotInitialized, "sceNpSignalingGetConnectionInfo", "a call before initialization did not return NOT_INITIALIZED first");
    Require(sceNpSignalingTerminate() == NotInitialized, "sceNpSignalingTerminate", "terminating before initialization did not return NOT_INITIALIZED");
    NpSignalingAccountPlatformPair account{0x1234, 1, 0};
    NpSignalingNetInfo netInfo{};
    netInfo.size = sizeof(netInfo);
    NpSignalingConnectionStatistics statistics{};
    std::uint32_t status = 7;
    std::uint32_t request = 7;
    int option = -7;
    Require(sceNpSignalingCreateContextA(-1, nullptr, nullptr, nullptr) == NotInitialized, "sceNpSignalingCreateContextA", "a call before initialization did not return NOT_INITIALIZED first");
    Require(sceNpSignalingActivateConnectionA(1, nullptr, nullptr) == NotInitialized, "sceNpSignalingActivateConnectionA", "a call before initialization did not return NOT_INITIALIZED first");
    Require(sceNpSignalingGetConnectionStatus(1, 1, nullptr, nullptr, nullptr) == NotInitialized, "sceNpSignalingGetConnectionStatus", "a call before initialization did not return NOT_INITIALIZED first");
    Require(sceNpSignalingGetConnectionInfoA(1, 1, 1, nullptr) == NotInitialized, "sceNpSignalingGetConnectionInfoA", "a call before initialization did not return NOT_INITIALIZED first");
    Require(sceNpSignalingGetConnectionFromNpId(1, nullptr, nullptr) == NotInitialized, "sceNpSignalingGetConnectionFromNpId", "a call before initialization did not return NOT_INITIALIZED first");
    Require(sceNpSignalingGetConnectionFromPeerAddress(1, 0, 0, nullptr) == NotInitialized, "sceNpSignalingGetConnectionFromPeerAddress", "a call before initialization did not return NOT_INITIALIZED first");
    Require(sceNpSignalingGetConnectionFromPeerAddressA(1, nullptr, nullptr) == NotInitialized, "sceNpSignalingGetConnectionFromPeerAddressA", "a call before initialization did not return NOT_INITIALIZED first");
    Require(sceNpSignalingGetContextOption(1, 1, nullptr) == NotInitialized, "sceNpSignalingGetContextOption", "a call before initialization did not return NOT_INITIALIZED first");
    Require(sceNpSignalingGetPeerNetInfoA(1, nullptr, nullptr) == NotInitialized, "sceNpSignalingGetPeerNetInfoA", "a call before initialization did not return NOT_INITIALIZED first");
    Require(sceNpSignalingCancelPeerNetInfo(1, 1) == NotInitialized, "sceNpSignalingCancelPeerNetInfo", "a call before initialization did not return NOT_INITIALIZED");
    Require(sceNpSignalingGetPeerNetInfoResult(1, 1, nullptr) == NotInitialized, "sceNpSignalingGetPeerNetInfoResult", "a call before initialization did not return NOT_INITIALIZED first");
    Require(sceNpSignalingGetConnectionStatistics(nullptr) == NotInitialized, "sceNpSignalingGetConnectionStatistics", "a call before initialization did not return NOT_INITIALIZED first");

    Require(sceNpSignalingInitialize(0, 0, 0, 0) == 0, "sceNpSignalingInitialize", "initialization failed");
    Require(sceNpSignalingInitialize(0, 0, 0, 0) == AlreadyInitialized, "sceNpSignalingInitialize", "a second initialization did not return ALREADY_INITIALIZED");

    Require(sceNpSignalingCreateContext(nullptr, nullptr, nullptr, &context) == InvalidArgument, "sceNpSignalingCreateContext", "a null SignalingNpId was taken");
    Require(sceNpSignalingCreateContext(&owner, nullptr, nullptr, nullptr) == InvalidArgument, "sceNpSignalingCreateContext", "a null output was taken");
    Require(sceNpSignalingCreateContext(&tooShort, nullptr, nullptr, &context) == InvalidArgument, "sceNpSignalingCreateContext", "a two-character online ID was taken");
    Require(sceNpSignalingCreateContext(&leadingUnderscore, nullptr, nullptr, &context) == InvalidArgument, "sceNpSignalingCreateContext", "an online ID starting with '_' was taken");
    Require(sceNpSignalingCreateContext(&withDot, nullptr, nullptr, &context) == InvalidArgument, "sceNpSignalingCreateContext", "an online ID with '.' was taken");
    Require(sceNpSignalingCreateContext(&unterminated, nullptr, nullptr, &context) == InvalidArgument, "sceNpSignalingCreateContext", "an unterminated online ID was taken");
    Require(sceNpSignalingCreateContext(&latin, nullptr, nullptr, &context) == InvalidArgument, "sceNpSignalingCreateContext", "an online ID with a byte outside ASCII was taken");
    Require(context == -7, "sceNpSignalingCreateContext", "a refused call wrote the context id");

    Require(sceNpSignalingCreateContext(&owner, nullptr, nullptr, &context) == 0 && context == 1, "sceNpSignalingCreateContext", "the first context is not id 1");
    Require(sceNpSignalingActivateConnection(context, &upper, &connection) == SignedOut, "sceNpSignalingActivateConnection", "an online ID with upper-case letters and digits was refused");
    int created = 0;
    Require(sceNpSignalingCreateContext(&maximum, nullptr, nullptr, &created) == 0 && created == 2, "sceNpSignalingCreateContext", "the second context is not id 2");
    Require(sceNpSignalingDeleteContext(2) == 0, "sceNpSignalingDeleteContext", "deleting context 2 failed");
    for (int expected = 3; expected <= 8; ++expected) {
        Require(sceNpSignalingCreateContext(&maximum, nullptr, nullptr, &created) == 0 && created == expected, "sceNpSignalingCreateContext", "a context id was reused before the ids up to 8 were handed out");
    }
    Require(sceNpSignalingCreateContext(&maximum, nullptr, nullptr, &created) == 0 && created == 2, "sceNpSignalingCreateContext", "after id 8 the allocation did not wrap to the free id 2");
    created = -7;
    Require(sceNpSignalingCreateContext(&owner, nullptr, nullptr, &created) == CtxIdNotAvailable && created == -7, "sceNpSignalingCreateContext", "a ninth context did not return CTXID_NOT_AVAILABLE");

    Require(sceNpSignalingSetContextOption(99, 2, 5) == CtxNotFound, "sceNpSignalingSetContextOption", "an unknown context was not refused before the option");
    Require(sceNpSignalingSetContextOption(context, 2, 0) == InvalidArgument, "sceNpSignalingSetContextOption", "option id 2 was taken");
    Require(sceNpSignalingSetContextOption(context, 1, 2) == InvalidArgument, "sceNpSignalingSetContextOption", "flag value 2 was taken");
    Require(sceNpSignalingSetContextOption(context, 1, 0) == 0, "sceNpSignalingSetContextOption", "clearing the flag failed");
    Require(sceNpSignalingSetContextOption(context, 1, 1) == 0, "sceNpSignalingSetContextOption", "setting the flag failed");

    Require(sceNpSignalingActivateConnection(context, nullptr, &connection) == InvalidArgument, "sceNpSignalingActivateConnection", "a null peer was taken");
    Require(sceNpSignalingActivateConnection(context, &peer, nullptr) == InvalidArgument, "sceNpSignalingActivateConnection", "a null output was taken");
    Require(sceNpSignalingActivateConnection(99, &tooShort, &connection) == InvalidArgument, "sceNpSignalingActivateConnection", "an invalid peer was not refused before the context");
    Require(sceNpSignalingActivateConnection(99, &peer, &connection) == CtxNotFound, "sceNpSignalingActivateConnection", "an unknown context was taken");
    Require(sceNpSignalingActivateConnection(context, &owner, &connection) == OwnNpId, "sceNpSignalingActivateConnection", "the context's own SignalingNpId did not return OWN_NP_ID");
    Require(sceNpSignalingActivateConnection(context, &peer, &connection) == SignedOut, "sceNpSignalingActivateConnection", "a valid peer did not return SIGNED_OUT");
    Require(connection == -7, "sceNpSignalingActivateConnection", "a connection id was written");

    Require(sceNpSignalingTerminateConnection(99, 1) == CtxNotFound, "sceNpSignalingTerminateConnection", "an unknown context was taken");
    Require(sceNpSignalingTerminateConnection(context, 1) == ConnNotFound, "sceNpSignalingTerminateConnection", "a connection that was never created was found");
    Require(sceNpSignalingDeactivateConnection(99, 1) == CtxNotFound, "sceNpSignalingDeactivateConnection", "an unknown context was taken");
    Require(sceNpSignalingDeactivateConnection(context, 1) == ConnNotFound, "sceNpSignalingDeactivateConnection", "a connection that was never created was found");
    Require(sceNpSignalingGetConnectionInfo(99, 1, 1, nullptr) == InvalidArgument, "sceNpSignalingGetConnectionInfo", "a null output was not refused before the context");
    Require(sceNpSignalingGetConnectionInfo(99, 1, 1, info) == CtxNotFound, "sceNpSignalingGetConnectionInfo", "an unknown context was taken");
    Require(sceNpSignalingGetConnectionInfo(context, 1, 1, info) == ConnNotFound, "sceNpSignalingGetConnectionInfo", "a connection that was never created was found");
    Require(info[0] == 0xA5 && info[63] == 0xA5, "sceNpSignalingGetConnectionInfo", "connection information was written");

    Require(sceNpSignalingCreateContextA(-1, nullptr, nullptr, &created) == InvalidArgument, "sceNpSignalingCreateContextA", "user id -1 was taken");
    Require(sceNpSignalingCreateContextA(1, nullptr, nullptr, nullptr) == InvalidArgument, "sceNpSignalingCreateContextA", "a null output was taken");
    created = -7;
    Require(sceNpSignalingCreateContextA(1, nullptr, nullptr, &created) == SignedOut && created == -7, "sceNpSignalingCreateContextA", "a signed-out user did not return SIGNED_OUT without a context");

    NpSignalingAccountPlatformPair noAccount{0, 1, 0};
    NpSignalingAccountPlatformPair noPlatform{0x1234, 0, 0};
    Require(sceNpSignalingActivateConnectionA(context, nullptr, &connection) == InvalidArgument, "sceNpSignalingActivateConnectionA", "a null peer was taken");
    Require(sceNpSignalingActivateConnectionA(context, &account, nullptr) == InvalidArgument, "sceNpSignalingActivateConnectionA", "a null output was taken");
    Require(sceNpSignalingActivateConnectionA(99, &noAccount, &connection) == InvalidArgument, "sceNpSignalingActivateConnectionA", "account id 0 was not refused before the context");
    Require(sceNpSignalingActivateConnectionA(99, &noPlatform, &connection) == InvalidArgument, "sceNpSignalingActivateConnectionA", "platform 0 was not refused before the context");
    Require(sceNpSignalingActivateConnectionA(99, &account, &connection) == CtxNotFound, "sceNpSignalingActivateConnectionA", "an unknown context was taken");
    Require(sceNpSignalingActivateConnectionA(context, &account, &connection) == SignedOut && connection == -7, "sceNpSignalingActivateConnectionA", "a valid peer did not return SIGNED_OUT without a connection");

    Require(sceNpSignalingGetConnectionStatus(99, 1, nullptr, nullptr, nullptr) == InvalidArgument, "sceNpSignalingGetConnectionStatus", "a null status was not refused before the context");
    Require(sceNpSignalingGetConnectionStatus(99, 1, &status, nullptr, nullptr) == CtxNotFound && status == 7, "sceNpSignalingGetConnectionStatus", "an unknown context was taken");
    std::uint32_t address = 0xA5A5A5A5u;
    std::uint16_t port = 0xA5A5u;
    Require(sceNpSignalingGetConnectionStatus(context, 1, &status, &address, &port) == 0 && status == 0, "sceNpSignalingGetConnectionStatus", "a connection that was never created is not INACTIVE");
    Require(address == 0xA5A5A5A5u && port == 0xA5A5u, "sceNpSignalingGetConnectionStatus", "an inactive connection wrote a peer address");

    Require(sceNpSignalingGetConnectionInfoA(99, 1, 1, nullptr) == InvalidArgument, "sceNpSignalingGetConnectionInfoA", "a null output was not refused before the context");
    Require(sceNpSignalingGetConnectionInfoA(99, 1, 1, info) == CtxNotFound, "sceNpSignalingGetConnectionInfoA", "an unknown context was taken");
    Require(sceNpSignalingGetConnectionInfoA(context, 1, 1, info) == ConnNotFound, "sceNpSignalingGetConnectionInfoA", "a connection that was never created was found");
    Require(info[0] == 0xA5 && info[63] == 0xA5, "sceNpSignalingGetConnectionInfoA", "connection information was written");

    Require(sceNpSignalingGetConnectionFromNpId(context, nullptr, &connection) == InvalidArgument, "sceNpSignalingGetConnectionFromNpId", "a null peer was taken");
    Require(sceNpSignalingGetConnectionFromNpId(context, &peer, nullptr) == InvalidArgument, "sceNpSignalingGetConnectionFromNpId", "a null output was taken");
    Require(sceNpSignalingGetConnectionFromNpId(99, &tooShort, &connection) == InvalidArgument, "sceNpSignalingGetConnectionFromNpId", "an invalid peer was not refused before the context");
    Require(sceNpSignalingGetConnectionFromNpId(99, &peer, &connection) == CtxNotFound, "sceNpSignalingGetConnectionFromNpId", "an unknown context was taken");
    Require(sceNpSignalingGetConnectionFromNpId(context, &peer, &connection) == ConnNotFound && connection == -7, "sceNpSignalingGetConnectionFromNpId", "a connection that was never created was found");

    Require(sceNpSignalingGetConnectionFromPeerAddress(99, 0x0100007Fu, 0x3412u, nullptr) == InvalidArgument, "sceNpSignalingGetConnectionFromPeerAddress", "a null output was not refused before the context");
    Require(sceNpSignalingGetConnectionFromPeerAddress(99, 0x0100007Fu, 0x3412u, &connection) == CtxNotFound, "sceNpSignalingGetConnectionFromPeerAddress", "an unknown context was taken");
    Require(sceNpSignalingGetConnectionFromPeerAddress(context, 0x0100007Fu, 0x3412u, &connection) == ConnNotFound && connection == -7, "sceNpSignalingGetConnectionFromPeerAddress", "a connection that was never created was found");

    Require(sceNpSignalingGetConnectionFromPeerAddressA(99, nullptr, &connection) == InvalidArgument, "sceNpSignalingGetConnectionFromPeerAddressA", "a null peer was not refused before the context");
    Require(sceNpSignalingGetConnectionFromPeerAddressA(99, &account, &connection) == CtxNotFound, "sceNpSignalingGetConnectionFromPeerAddressA", "an unknown context was taken");
    Require(sceNpSignalingGetConnectionFromPeerAddressA(context, &account, &connection) == InvalidArgument && connection == -7, "sceNpSignalingGetConnectionFromPeerAddressA", "a context created from an NpId was taken");

    Require(sceNpSignalingGetContextOption(99, 2, nullptr) == InvalidArgument, "sceNpSignalingGetContextOption", "a null output was not refused before the context");
    Require(sceNpSignalingGetContextOption(99, 2, &option) == CtxNotFound, "sceNpSignalingGetContextOption", "an unknown context was not refused before the option");
    Require(sceNpSignalingGetContextOption(context, 2, &option) == InvalidArgument && option == -7, "sceNpSignalingGetContextOption", "option id 2 was taken");
    Require(sceNpSignalingGetContextOption(context, 1, &option) == 0 && option == 1, "sceNpSignalingGetContextOption", "the flag set above does not read 1");
    Require(sceNpSignalingSetContextOption(context, 1, 0) == 0, "sceNpSignalingSetContextOption", "clearing the flag failed");
    Require(sceNpSignalingGetContextOption(context, 1, &option) == 0 && option == 0, "sceNpSignalingGetContextOption", "the cleared flag does not read 0");
    Require(sceNpSignalingSetContextOption(context, 1, 1) == 0, "sceNpSignalingSetContextOption", "setting the flag failed");

    Require(sceNpSignalingGetPeerNetInfoA(99, nullptr, &request) == InvalidArgument, "sceNpSignalingGetPeerNetInfoA", "a null peer was not refused before the context");
    Require(sceNpSignalingGetPeerNetInfoA(99, &account, &request) == CtxNotFound, "sceNpSignalingGetPeerNetInfoA", "an unknown context was taken");
    Require(sceNpSignalingGetPeerNetInfoA(context, &account, &request) == InvalidArgument && request == 7, "sceNpSignalingGetPeerNetInfoA", "a context created from an NpId was taken");

    Require(sceNpSignalingCancelPeerNetInfo(99, 1) == CtxNotFound, "sceNpSignalingCancelPeerNetInfo", "an unknown context was taken");
    Require(sceNpSignalingCancelPeerNetInfo(context, 1) == ReqNotFound, "sceNpSignalingCancelPeerNetInfo", "a request that was never made was found");

    Require(sceNpSignalingGetPeerNetInfoResult(99, 1, nullptr) == InvalidArgument, "sceNpSignalingGetPeerNetInfoResult", "a null output was not refused before the context");
    netInfo.size = 0;
    Require(sceNpSignalingGetPeerNetInfoResult(99, 1, &netInfo) == CtxNotFound, "sceNpSignalingGetPeerNetInfoResult", "an unknown context was not refused before the size");
    Require(sceNpSignalingGetPeerNetInfoResult(context, 1, &netInfo) == InvalidArgument, "sceNpSignalingGetPeerNetInfoResult", "size 0 was taken");
    netInfo.size = sizeof(netInfo);
    Require(sceNpSignalingGetPeerNetInfoResult(context, 1, &netInfo) == ResultNotFound && netInfo.localAddr == 0, "sceNpSignalingGetPeerNetInfoResult", "a result that was never staged was found");

    std::memset(&statistics, 0xA5, sizeof(statistics));
    Require(sceNpSignalingGetConnectionStatistics(nullptr) == InvalidArgument, "sceNpSignalingGetConnectionStatistics", "a null output was taken");
    Require(sceNpSignalingGetConnectionStatistics(&statistics) == 0, "sceNpSignalingGetConnectionStatistics", "the call failed");
    Require(statistics.peakConnectionCount == 0 && statistics.activeConnectionCount == 0 && statistics.transientConnectionCount == 0 && statistics.establishedConnectionCount == 0, "sceNpSignalingGetConnectionStatistics", "a connection count is not 0");

    Require(sceNpSignalingDeleteContext(99) == 0, "sceNpSignalingDeleteContext", "deleting an unknown context failed");
    Require(sceNpSignalingDeleteContext(context) == 0, "sceNpSignalingDeleteContext", "deleting a context failed");
    Require(sceNpSignalingSetContextOption(context, 1, 1) == CtxNotFound, "sceNpSignalingSetContextOption", "a deleted context was found");

    Require(sceNpSignalingTerminate() == 0, "sceNpSignalingTerminate", "termination failed");
    Require(sceNpSignalingCreateContext(&owner, nullptr, nullptr, &created) == NotInitialized, "sceNpSignalingCreateContext", "a context was created after termination");
    Require(sceNpSignalingInitialize(0, 0, 0, 0) == 0, "sceNpSignalingInitialize", "initializing again failed");
    Require(sceNpSignalingSetContextOption(2, 1, 1) == CtxNotFound, "sceNpSignalingTerminate", "a context created before termination is still found");
    Require(sceNpSignalingCreateContext(&owner, nullptr, nullptr, &created) == 0 && created == 1, "sceNpSignalingCreateContext", "termination did not release the contexts and restart the ids at 1");
    Require(sceNpSignalingTerminate() == 0, "sceNpSignalingTerminate", "the second termination failed");
    Require(sceNpSignalingTerminate() == NotInitialized, "sceNpSignalingTerminate", "terminating when already terminated did not return NOT_INITIALIZED");
    Require(sceNpSignalingDeleteContext(1) == NotInitialized, "sceNpSignalingDeleteContext", "deleting context post-termination did not return NOT_INITIALIZED");
    Require(sceNpSignalingActivateConnection(1, nullptr, nullptr) == NotInitialized, "sceNpSignalingActivateConnection", "activating connection post-termination did not return NOT_INITIALIZED");
    Require(sceNpSignalingTerminateConnection(1, 1) == NotInitialized, "sceNpSignalingTerminateConnection", "terminating connection post-termination did not return NOT_INITIALIZED");
    Require(sceNpSignalingDeactivateConnection(1, 1) == NotInitialized, "sceNpSignalingDeactivateConnection", "deactivating connection post-termination did not return NOT_INITIALIZED");
    Require(sceNpSignalingGetConnectionInfo(1, 1, 1, nullptr) == NotInitialized, "sceNpSignalingGetConnectionInfo", "getting connection info post-termination did not return NOT_INITIALIZED");

    if (failures != 0) return 1;
    std::puts("NpSignaling offline tests passed");
    return 0;
}
