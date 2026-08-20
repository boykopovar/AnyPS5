#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceNpAuthAbortRequest(int req_id) {
 (void)req_id;
 return 0;
}

int sceNpAuthCreateAsyncRequest(const void* param) {
 (void)param;
 return 0;
}

int sceNpAuthCreateRequest(void) {
 return 0;
}

int sceNpAuthDeleteRequest(int req_id) {
 (void)req_id;
 return 0;
}

int sceNpAuthGetAuthorizationCodeV3(int req_id, const void* param, void* auth_code, int* issuer_id) {
 (void)req_id;
 (void)param;
 (void)auth_code;
 (void)issuer_id;
 return 0;
}

int sceNpAuthGetIdTokenV3(int req_id, const void* param, void* id_token) {
 (void)req_id;
 (void)param;
 (void)id_token;
 return 0;
}

int sceNpAuthPollAsync(int req_id, int* result) {
 (void)req_id;
 (void)result;
 return 0;
}

int sceNpAuthWaitAsync(int req_id, int* result) {
 (void)req_id;
 (void)result;
 return 0;
}

}
