#include <cstdint>
#include <cstddef>
#include <atomic>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int SCE_NP_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550003);
static constexpr int SCE_NP_ERROR_SIGNED_OUT = static_cast<int>(0x80550006);
static constexpr int NP_POLL_ASYNC_FINISHED = 0;

static std::atomic<int> g_nextRequest{1};

extern "C" {

int APS5_VABI sceNpAuthAbortRequest(int req_id) {
 (void)req_id;
 return 0;
}

int APS5_VABI sceNpAuthCreateAsyncRequest(const void* param) {
 (void)param;
 return g_nextRequest.fetch_add(1, std::memory_order_relaxed);
}

int APS5_VABI sceNpAuthCreateRequest(void) {
 return g_nextRequest.fetch_add(1, std::memory_order_relaxed);
}

int APS5_VABI sceNpAuthDeleteRequest(int req_id) {
 (void)req_id;
 return 0;
}

int APS5_VABI sceNpAuthGetAuthorizationCodeV3(int req_id, const void* param, void* auth_code, int* issuer_id) {
 (void)req_id;
 (void)issuer_id;
 if (!param || !auth_code) return SCE_NP_ERROR_INVALID_ARGUMENT;
 return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpAuthGetIdTokenV3(int req_id, const void* param, void* id_token) {
 (void)req_id;
 if (!param || !id_token) return SCE_NP_ERROR_INVALID_ARGUMENT;
 return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpAuthPollAsync(int req_id, int* result) {
 (void)req_id;
 if (result) *result = SCE_NP_ERROR_SIGNED_OUT;
 return NP_POLL_ASYNC_FINISHED;
}

int APS5_VABI sceNpAuthWaitAsync(int req_id, int* result) {
 return sceNpAuthPollAsync(req_id, result);
}

}
