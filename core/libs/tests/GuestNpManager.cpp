#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceNpGetNpId(int user_id, NpId* np_id);
int APS5_VABI sceNpGetUserIdByAccountId(std::uint64_t account_id, int* user_id);
int APS5_VABI sceNpUnknown_8weiQyIdI3A(int userId, void* buffer);
int APS5_VABI sceNpUnknown_WxLcN76q19o(void);
}

extern "C" {
int APS5_VABI sceNpCreateRequest(void);
int APS5_VABI sceNpCreateAsyncRequest(const NpCreateAsyncRequestParameter* param);
int APS5_VABI sceNpDeleteRequest(int reqId);
int APS5_VABI sceNpSetTimeout(int reqId, std::int32_t resolveRetry, std::uint32_t resolveTimeout,
    std::uint32_t connTimeout, std::uint32_t sendTimeout, std::uint32_t recvTimeout);
int APS5_VABI sceNpSetContentRestriction(const NpContentRestriction* restriction);
void APS5_VABI sceNpRegisterGamePresenceCallback(void* callback, void* userdata);
}

namespace {

constexpr int InvalidArgument = static_cast<int>(0x80550003u);
constexpr int SignedOut = static_cast<int>(0x80550006u);
constexpr int RequestNotFound = static_cast<int>(0x80550014u);
constexpr int InvalidSize = static_cast<int>(0x80550011u);

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "NpManager: %s\n", message);
        std::abort();
    }
}

void CheckRequestTimeouts() {
    Require(sceNpSetTimeout(0x7fffffff, 2, 0, 0, 0, 0) == RequestNotFound, "never created request");
    Require(sceNpSetTimeout(0x7fffffff, 0, 0, 0, 0, 0) == InvalidArgument, "arguments checked before the request");
    const int requestId = sceNpCreateRequest();
    Require(requestId > 0, "sceNpCreateRequest must return a positive request id");
    Require(sceNpSetTimeout(requestId, 2, 1000000, 10000000, 10000000, 10000000) == 0, "minimum timeouts");
    Require(sceNpSetTimeout(requestId, 0, 0, 0, 0, 30000000) == 0, "only the receive timeout");
    Require(sceNpSetTimeout(requestId, 3, 0, 0, 0, 0) == 0, "only the resolve retries");
    Require(sceNpSetTimeout(requestId, 0, 0, 0, 0, 0) == InvalidArgument, "every value left at its default");
    Require(sceNpSetTimeout(0, 2, 0, 0, 0, 0) == InvalidArgument, "request id 0");
    Require(sceNpSetTimeout(-1, 2, 0, 0, 0, 0) == InvalidArgument, "negative request id");
    Require(sceNpSetTimeout(requestId, -1, 0, 0, 0, 30000000) == InvalidArgument, "negative resolve retries");
    Require(sceNpSetTimeout(requestId, 0, 999999, 0, 0, 0) == InvalidArgument, "resolve timeout below 1 s");
    Require(sceNpSetTimeout(requestId, 0, 0, 9999999, 0, 0) == InvalidArgument, "connect timeout below 10 s");
    Require(sceNpSetTimeout(requestId, 0, 0, 0, 9999999, 0) == InvalidArgument, "send timeout below 10 s");
    Require(sceNpSetTimeout(requestId, 0, 0, 0, 0, 1) == InvalidArgument, "receive timeout below 10 s");
    Require(sceNpDeleteRequest(requestId) == 0, "sceNpDeleteRequest failed");
    Require(sceNpSetTimeout(requestId, 2, 0, 0, 0, 0) == RequestNotFound, "deleted request");
    NpCreateAsyncRequestParameter param{};
    const int asyncId = sceNpCreateAsyncRequest(&param);
    Require(asyncId > 0, "sceNpCreateAsyncRequest must return a positive request id");
    Require(sceNpSetTimeout(asyncId, 2, 0, 0, 0, 0) == 0, "async request");
    Require(sceNpDeleteRequest(asyncId) == 0, "sceNpDeleteRequest failed on the async request");
    Require(sceNpSetTimeout(asyncId, 2, 0, 0, 0, 0) == RequestNotFound, "deleted async request");
}

void CheckContentRestriction() {
    NpAgeRestriction ages[0x100]{};
    ages[0] = {{'u', 's', 0, 0}, 13, {}};
    ages[1] = {{'j', 'p', 0, 0}, 15, {}};
    NpContentRestriction restriction{};
    restriction.size = sizeof(restriction);
    restriction.defaultAgeRestriction = 0;
    Require(sceNpSetContentRestriction(&restriction) == 0, "an empty restriction");
    restriction.ageRestrictionCount = 2;
    restriction.ageRestriction = ages;
    Require(sceNpSetContentRestriction(&restriction) == 0, "two age restrictions");
    restriction.ageRestrictionCount = 0x100;
    Require(sceNpSetContentRestriction(&restriction) == 0, "256 age restrictions");
    restriction.ageRestrictionCount = 0x101;
    Require(sceNpSetContentRestriction(&restriction) == InvalidArgument, "more than 256 age restrictions");
    restriction.ageRestrictionCount = 1;
    restriction.ageRestriction = nullptr;
    Require(sceNpSetContentRestriction(&restriction) == InvalidArgument, "a count with no array");
    restriction.ageRestrictionCount = 0;
    Require(sceNpSetContentRestriction(&restriction) == 0, "no count and no array");
    restriction.ageRestrictionCount = -1;
    Require(sceNpSetContentRestriction(&restriction) == 0, "a negative count is accepted");
    restriction.ageRestrictionCount = 0;
    restriction.defaultAgeRestriction = -1;
    Require(sceNpSetContentRestriction(&restriction) == InvalidArgument, "a negative default age");
    restriction.defaultAgeRestriction = 18;
    restriction.size = sizeof(restriction) + 8;
    Require(sceNpSetContentRestriction(&restriction) == InvalidSize, "a larger size");
    restriction.size = 0;
    Require(sceNpSetContentRestriction(&restriction) == InvalidSize, "a zero size");
    restriction.defaultAgeRestriction = -1;
    Require(sceNpSetContentRestriction(&restriction) == InvalidSize, "the size is checked before the ages");
    Require(sceNpSetContentRestriction(nullptr) == InvalidArgument, "a null restriction");
}

void APS5_VABI GamePresence(const NpOnlineId*, void* userdata) {
    ++*static_cast<int*>(userdata);
}

void CheckGamePresenceCallback() {
    int first = 0;
    int second = 0;
    sceNpRegisterGamePresenceCallback(reinterpret_cast<void*>(&GamePresence), &first);
    Require(first == 0, "registering does not call the presence callback");
    sceNpRegisterGamePresenceCallback(reinterpret_cast<void*>(&GamePresence), &second);
    Require(first == 0 && second == 0, "replacing does not call either presence callback");
    sceNpRegisterGamePresenceCallback(nullptr, nullptr);
    Require(first == 0 && second == 0, "clearing does not call the presence callback");
}

}

int main() {
    CheckRequestTimeouts();
    CheckContentRestriction();
    CheckGamePresenceCallback();
    NpId npId{};
    std::memset(&npId, 0x5a, sizeof(npId));
    NpId untouched{};
    std::memset(&untouched, 0x5a, sizeof(untouched));
    Require(sceNpGetNpId(0x10000, &npId) == SignedOut, "sceNpGetNpId must report the user as signed out");
    Require(std::memcmp(&npId, &untouched, sizeof(npId)) == 0, "sceNpGetNpId must leave the NpId untouched");
    Require(sceNpGetNpId(0x10000, nullptr) == InvalidArgument, "sceNpGetNpId must reject a null NpId");
    int userId = 0x5a5a;
    Require(sceNpGetUserIdByAccountId(0, &userId) == InvalidArgument, "sceNpGetUserIdByAccountId must reject account id 0");
    Require(sceNpGetUserIdByAccountId(0x1234, nullptr) == InvalidArgument, "sceNpGetUserIdByAccountId must reject a null output");
    Require(sceNpGetUserIdByAccountId(0x1234, &userId) == SignedOut, "sceNpGetUserIdByAccountId must report NP as signed out");
    Require(userId == 0x5a5a, "sceNpGetUserIdByAccountId must leave the user id untouched");
    unsigned char buffer[0x11]{};
    try {
        sceNpUnknown_8weiQyIdI3A(0x10000, buffer);
        Require(false, "sceNpUnknown_8weiQyIdI3A must report that it is not implemented");
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()) == "sceNpUnknown_8weiQyIdI3A not implemented",
            "sceNpUnknown_8weiQyIdI3A threw");
    }
    try {
        sceNpUnknown_WxLcN76q19o();
        Require(false, "sceNpUnknown_WxLcN76q19o must report that it is not implemented");
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()) == "sceNpUnknown_WxLcN76q19o not implemented",
            "sceNpUnknown_WxLcN76q19o threw");
    }
    return 0;
}
