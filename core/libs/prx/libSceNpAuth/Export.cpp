#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpAuthAbortRequest(int req_id) {
 (void)req_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpAuthCreateAsyncRequest(const void* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpAuthCreateRequest(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpAuthDeleteRequest(int req_id) {
 (void)req_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpAuthGetAuthorizationCodeV3(int req_id, const void* param, void* auth_code, int* issuer_id) {
 (void)req_id;
 (void)param;
 (void)auth_code;
 (void)issuer_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpAuthGetIdTokenV3(int req_id, const void* param, void* id_token) {
 (void)req_id;
 (void)param;
 (void)id_token;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpAuthPollAsync(int req_id, int* result) {
 (void)req_id;
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNpAuthWaitAsync(int req_id, int* result) {
 (void)req_id;
 (void)result;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
