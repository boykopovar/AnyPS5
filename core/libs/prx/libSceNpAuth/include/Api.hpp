#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpAuthAbortRequest(int reqId);
int APS5_VABI sceNpAuthCreateAsyncRequest(const void* param);
int APS5_VABI sceNpAuthCreateRequest(void);
int APS5_VABI sceNpAuthDeleteRequest(int reqId);
int APS5_VABI sceNpAuthGetAuthorizedAppCode(std::int32_t requestId, const NpAuthAuthorizedAppCodeParameter* parameter, char* code, std::int32_t* issuerId, bool* authorizationRequired);
int APS5_VABI sceNpAuthGetAuthorizationCodeV3(int reqId, const void* param, void* authCode, int* issuerId);
int APS5_VABI sceNpAuthGetIdTokenV3(int reqId, const void* param, void* idToken);
int APS5_VABI sceNpAuthGetAuthorizationCode(int reqId, const void* param, void* authCode);
int APS5_VABI sceNpAuthGetIdToken(int reqId, const void* param, void* idToken);
int APS5_VABI sceNpAuthPollAsync(int reqId, int* result);
int APS5_VABI sceNpAuthSetTimeout(std::int32_t requestId, std::int32_t resolveRetry, std::uint32_t resolveTimeout, std::uint32_t connectTimeout, std::uint32_t sendTimeout, std::uint32_t receiveTimeout);
int APS5_VABI sceNpAuthWaitAsync(int reqId, int* result);
int APS5_VABI sceNpAuthGetAuthorizationCodeA(int reqId, const void* param, void* authCode, int* issuerId);
int APS5_VABI sceNpAuthGetIdTokenA(int reqId, const void* param, void* token);

}
