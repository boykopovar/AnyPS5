#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpAbortRequest(int req_id) {
 (void)req_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpCheckCallback(void) {
 NotImplemented_nid_no_patch(__func__);
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
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpCreateAsyncRequest(const NpCreateAsyncRequestParameter* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpCreateRequest(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpDeleteRequest(int req_id) {
 (void)req_id;
 NotImplemented_nid_no_patch(__func__);
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
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpGetAccountIdA(int user_id, uint64_t* account_id) {
 (void)user_id;
 (void)account_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
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
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpGetState(int user_id, uint32_t* state) {
 (void)user_id;
 (void)state;
 NotImplemented_nid_no_patch(__func__);
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
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
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
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpUnregisterStateCallback(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
