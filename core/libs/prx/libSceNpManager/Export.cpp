#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>

// PSN is not emulated: the user is reported as signed out and online queries fail.
static constexpr int SCE_NP_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550003);
static constexpr int SCE_NP_ERROR_SIGNED_OUT = static_cast<int>(0x80550006);
static constexpr uint32_t NP_STATE_SIGNED_OUT = 1;
static constexpr int NP_POLL_ASYNC_FINISHED = 0;

static std::atomic<int> g_nextRequest{1};

extern "C" {

int APS5_VABI sceNpAbortRequest(int req_id) {
    (void)req_id;
    return 0;
}

int APS5_VABI sceNpCheckCallback(void) {
    return 0;
}

int APS5_VABI sceNpCheckNpAvailability(int req_id, const char* user, void* result) {
 (void)req_id;
 (void)user;
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpCheckNpReachability(int req_id, int user_id) {
 (void)req_id;
 (void)user_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpCheckPremium(int req_id, const NpCheckPremiumParameter* param, NpCheckPremiumResult* result) {
    (void)req_id;
    (void)param;
    (void)result;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpCreateAsyncRequest(const NpCreateAsyncRequestParameter* param) {
    (void)param;
    return g_nextRequest.fetch_add(1, std::memory_order_relaxed);
}

int APS5_VABI sceNpCreateRequest(void) {
    return g_nextRequest.fetch_add(1, std::memory_order_relaxed);
}

int APS5_VABI sceNpDeleteRequest(int req_id) {
    (void)req_id;
    return 0;
}

int APS5_VABI sceNpGetAccountAge(int req_id, int user_id, uint8_t* age) {
 (void)req_id;
 (void)user_id;
 (void)age;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpGetAccountCountryA(int user_id, void* country_code) {
    (void)user_id;
    (void)country_code;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpGetAccountIdA(int user_id, uint64_t* account_id) {
    (void)user_id;
    (void)account_id;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpGetNpId(int user_id, NpId* np_id) {
 (void)user_id;
 (void)np_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpGetNpReachabilityState(int user_id, uint32_t* state) {
 (void)user_id;
 (void)state;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpGetOnlineId(int user_id, NpOnlineId* online_id) {
    (void)user_id;
    (void)online_id;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpGetState(int user_id, uint32_t* state) {
    (void)user_id;
    if (!state) return SCE_NP_ERROR_INVALID_ARGUMENT;
    *state = NP_STATE_SIGNED_OUT;
    return 0;
}

int APS5_VABI sceNpHasSignedUp(int user_id, bool* has_signed_up) {
 (void)user_id;
 (void)has_signed_up;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpPollAsync(int req_id, int* result) {
    (void)req_id;
    if (result) *result = SCE_NP_ERROR_SIGNED_OUT;
    return NP_POLL_ASYNC_FINISHED;
}

void APS5_VABI sceNpRegisterGamePresenceCallback(void* callback, void* userdata) {
 (void)callback;
 (void)userdata;
 NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpRegisterNpReachabilityStateCallback(void* callback, void* userdata) {
 (void)callback;
 (void)userdata;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpRegisterPlusEventCallback(void* callback, void* userdata) {
 (void)callback;
 (void)userdata;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpRegisterPremiumEventCallback(void* callback, void* userdata) {
 (void)callback;
 (void)userdata;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpRegisterStateCallback(void* callback, void* userdata) {
 (void)callback;
 (void)userdata;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpSetContentRestriction(const NpContentRestriction* restriction) {
 (void)restriction;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpSetNpTitleId(const NpTitleId* title_id, const NpTitleSecret* title_secret) {
    (void)title_id;
    (void)title_secret;
    return 0;
}

int APS5_VABI sceNpUnregisterStateCallback(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpGetAccountLanguage2(int req_id, int user_id, void* language) {
    (void)req_id;
    (void)user_id;
    (void)language;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpNotifyPremiumFeature(const void* param) {
    (void)param;
    return 0;
}

int APS5_VABI sceNpRegisterStateCallbackA(void* callback, void* userdata) {
    (void)userdata;
    if (!callback) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return 1;
}

int APS5_VABI sceNpUnregisterStateCallbackA(int callback_id) {
    (void)callback_id;
    return 0;
}

}
