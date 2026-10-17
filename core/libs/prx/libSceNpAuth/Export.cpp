#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static constexpr int ORBIS_NP_AUTH_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550301);
static constexpr int SCE_NP_ERROR_SIGNED_OUT = static_cast<int>(0x80550006);
static constexpr int ORBIS_NP_AUTH_ERROR_ABORTED = static_cast<int>(0x80550304);
static constexpr int ORBIS_NP_AUTH_ERROR_REQUEST_NOT_FOUND = static_cast<int>(0x80550306);
static constexpr int ORBIS_NP_AUTH_ERROR_INVALID_ID = static_cast<int>(0x80550307);
static constexpr int NP_POLL_ASYNC_FINISHED = 0;
static constexpr int NP_POLL_ASYNC_RUNNING = 1;

namespace {

struct RequestInfo {
 bool isAsync = false;
 bool processed = false;
 bool aborted = false;
 bool polled = false;
 int result = SCE_NP_ERROR_SIGNED_OUT;
};

std::mutex g_requestMutex;
std::unordered_map<int, RequestInfo> g_requests;
std::atomic<int> g_nextRequest{1};

int CreateRequest(bool isAsync) {
 const int reqId = g_nextRequest.fetch_add(1, std::memory_order_relaxed);
 std::lock_guard lock(g_requestMutex);
 g_requests[reqId] = {isAsync, false, false, false, SCE_NP_ERROR_SIGNED_OUT};
 return reqId;
}

}

extern "C" {

int APS5_VABI sceNpAuthAbortRequest(int req_id) {
 std::lock_guard lock(g_requestMutex);
 auto it = g_requests.find(req_id);
 if (it == g_requests.end()) return ORBIS_NP_AUTH_ERROR_REQUEST_NOT_FOUND;
 if (!it->second.processed) it->second.aborted = true;
 return 0;
}

int APS5_VABI sceNpAuthCreateAsyncRequest(const void* param) {
 (void)param;
 return CreateRequest(true);
}

int APS5_VABI sceNpAuthCreateRequest(void) {
 return CreateRequest(false);
}

int APS5_VABI sceNpAuthDeleteRequest(int req_id) {
 std::lock_guard lock(g_requestMutex);
 g_requests.erase(req_id);
 return 0;
}

int APS5_VABI sceNpAuthGetAuthorizedAppCode() {
 return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpAuthGetAuthorizationCodeV3(int req_id, const void* param, void* auth_code, int* issuer_id) {
 (void)issuer_id;
 if (!param || !auth_code) return ORBIS_NP_AUTH_ERROR_INVALID_ARGUMENT;
 std::lock_guard lock(g_requestMutex);
 auto it = g_requests.find(req_id);
 if (it == g_requests.end()) return ORBIS_NP_AUTH_ERROR_REQUEST_NOT_FOUND;
 if (it->second.processed) return ORBIS_NP_AUTH_ERROR_INVALID_ARGUMENT;
 if (it->second.aborted) {
  it->second.result = ORBIS_NP_AUTH_ERROR_ABORTED;
  return ORBIS_NP_AUTH_ERROR_ABORTED;
 }
 it->second.processed = true;
 if (it->second.isAsync) {
  it->second.result = SCE_NP_ERROR_SIGNED_OUT;
  return 0;
 }
 return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpAuthGetIdTokenV3(int req_id, const void* param, void* id_token) {
 if (!param || !id_token) return ORBIS_NP_AUTH_ERROR_INVALID_ARGUMENT;
 std::lock_guard lock(g_requestMutex);
 auto it = g_requests.find(req_id);
 if (it == g_requests.end()) return ORBIS_NP_AUTH_ERROR_REQUEST_NOT_FOUND;
 if (it->second.processed) return ORBIS_NP_AUTH_ERROR_INVALID_ARGUMENT;
 if (it->second.aborted) {
  it->second.result = ORBIS_NP_AUTH_ERROR_ABORTED;
  return ORBIS_NP_AUTH_ERROR_ABORTED;
 }
 it->second.processed = true;
 if (it->second.isAsync) {
  it->second.result = SCE_NP_ERROR_SIGNED_OUT;
  return 0;
 }
 return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpAuthPollAsync(int req_id, int* result) {
 std::lock_guard lock(g_requestMutex);
 auto it = g_requests.find(req_id);
 if (it == g_requests.end()) return ORBIS_NP_AUTH_ERROR_REQUEST_NOT_FOUND;
 if (!it->second.isAsync || (!it->second.processed && !it->second.aborted)) return ORBIS_NP_AUTH_ERROR_INVALID_ID;
 if (!it->second.polled) {
  it->second.polled = true;
  return NP_POLL_ASYNC_RUNNING;
 }
 if (result) *result = it->second.result;
 return NP_POLL_ASYNC_FINISHED;
}

int APS5_VABI sceNpAuthSetTimeout(int req_id, int resolve_retry, uint32_t resolve_timeout, uint32_t connect_timeout, uint32_t send_timeout, uint32_t receive_timeout) {
 (void)resolve_retry;
 (void)resolve_timeout;
 (void)connect_timeout;
 (void)send_timeout;
 (void)receive_timeout;
 std::lock_guard lock(g_requestMutex);
 return g_requests.contains(req_id) ? 0 : ORBIS_NP_AUTH_ERROR_REQUEST_NOT_FOUND;
}

int APS5_VABI sceNpAuthWaitAsync(int req_id, int* result) {
 if (!result) return ORBIS_NP_AUTH_ERROR_INVALID_ARGUMENT;
 std::lock_guard lock(g_requestMutex);
 auto it = g_requests.find(req_id);
 if (it == g_requests.end()) return ORBIS_NP_AUTH_ERROR_REQUEST_NOT_FOUND;
 if (!it->second.isAsync || (!it->second.processed && !it->second.aborted)) return ORBIS_NP_AUTH_ERROR_INVALID_ID;
 *result = it->second.result;
 return NP_POLL_ASYNC_FINISHED;
}

}

extern "C" {

int APS5_VABI sceNpAuthGetAuthorizationCode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
