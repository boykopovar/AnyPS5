#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int sceNpAbortRequest(int req_id) {
 (void)req_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpCheckCallback(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpCheckNpAvailability(int req_id, const char* user, void* result) {
 (void)req_id;
 (void)user;
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpCheckNpReachability(int req_id, int user_id) {
 (void)req_id;
 (void)user_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpCheckPremium(int req_id, const NpCheckPremiumParameter* param, NpCheckPremiumResult* result) {
 (void)req_id;
 (void)param;
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpCreateAsyncRequest(const NpCreateAsyncRequestParameter* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpCreateRequest(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpDeleteRequest(int req_id) {
 (void)req_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpGetAccountAge(int req_id, int user_id, uint8_t* age) {
 (void)req_id;
 (void)user_id;
 (void)age;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpGetAccountCountryA(int user_id, void* country_code) {
 (void)user_id;
 (void)country_code;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpGetAccountIdA(int user_id, uint64_t* account_id) {
 (void)user_id;
 (void)account_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpGetNpId(int user_id, NpId* np_id) {
 (void)user_id;
 (void)np_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpGetNpReachabilityState(int user_id, uint32_t* state) {
 (void)user_id;
 (void)state;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpGetOnlineId(int user_id, NpOnlineId* online_id) {
 (void)user_id;
 (void)online_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpGetState(int user_id, uint32_t* state) {
 (void)user_id;
 (void)state;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpHasSignedUp(int user_id, bool* has_signed_up) {
 (void)user_id;
 (void)has_signed_up;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpPollAsync(int req_id, int* result) {
 (void)req_id;
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void sceNpRegisterGamePresenceCallback(void* callback, void* userdata) {
 (void)callback;
 (void)userdata;
 NotImplemented_nid_no_patch(__func__);
}

int sceNpRegisterNpReachabilityStateCallback(void* callback, void* userdata) {
 (void)callback;
 (void)userdata;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpRegisterPlusEventCallback(void* callback, void* userdata) {
 (void)callback;
 (void)userdata;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpRegisterPremiumEventCallback(void* callback, void* userdata) {
 (void)callback;
 (void)userdata;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpRegisterStateCallback(void* callback, void* userdata) {
 (void)callback;
 (void)userdata;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpSetContentRestriction(const NpContentRestriction* restriction) {
 (void)restriction;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpSetNpTitleId(const NpTitleId* title_id, const NpTitleSecret* title_secret) {
 (void)title_id;
 (void)title_secret;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceNpUnregisterStateCallback(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
