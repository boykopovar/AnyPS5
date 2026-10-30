#pragma once

#include "SceTypes.hpp"

struct NpWebApiContentParameter {
    std::uint64_t contentLength;
    const char* contentType;
    std::uint8_t reserved[16];
};

static_assert(sizeof(NpWebApiContentParameter) == 32);

extern "C" {

int APS5_VABI sceNpWebApiAbortRequest(std::int64_t requestId);
int APS5_VABI sceNpWebApiCreateContext(int libCtxId, const void* onlineId);
int APS5_VABI sceNpWebApiCreateHandle(int libCtxId);
int APS5_VABI sceNpWebApiCreatePushEventFilter(int libCtxId, const void* filterParam, std::size_t filterParamNum);
int APS5_VABI sceNpWebApiCreateRequest(int titleUserCtxId, const char* apiGroup, const char* path, int method, const NpWebApiContentParameter* contentParameter, std::int64_t* requestId);
int APS5_VABI sceNpWebApiCreateServicePushEventFilter(int libCtxId, int handleId, const char* npServiceName, std::uint32_t npServiceLabel, const void* filterParam, std::size_t filterParamNum);
int APS5_VABI sceNpWebApiDeleteContext(int titleUserCtxId);
int APS5_VABI sceNpWebApiDeleteHandle(int libCtxId, int handleId);
int APS5_VABI sceNpWebApiDeletePushEventFilter(int libCtxId, int filterId);
int APS5_VABI sceNpWebApiDeleteRequest(std::int64_t requestId);
int APS5_VABI sceNpWebApiDeleteServicePushEventFilter(int libCtxId, int filterId);
int APS5_VABI sceNpWebApiGetHttpResponseHeaderValue();
int APS5_VABI sceNpWebApiGetHttpResponseHeaderValueLength();
int APS5_VABI sceNpWebApiGetHttpStatusCode(std::int64_t requestId, int* statusCode);
int APS5_VABI sceNpWebApiInitialize(int libHttpCtxId, std::size_t poolSize);
int APS5_VABI sceNpWebApiReadData(std::int64_t requestId, void* data, std::size_t size);
int APS5_VABI sceNpWebApiRegisterPushEventCallback(int titleUserCtxId, int filterId, void* callback, void* userArg);
int APS5_VABI sceNpWebApiRegisterServicePushEventCallback(int titleUserCtxId, int filterId, void* callback, void* userArg);
int APS5_VABI sceNpWebApiSendRequest(std::int64_t requestId, const void* data, std::size_t dataSize);
int APS5_VABI sceNpWebApiTerminate(int libCtxId);
int APS5_VABI sceNpWebApiUnregisterPushEventCallback(int titleUserCtxId, int callbackId);
int APS5_VABI sceNpWebApiUnregisterServicePushEventCallback(int titleUserCtxId, int callbackId);
int APS5_VABI sceNpWebApiUtilityParseNpId();

int APS5_VABI sceNpWebApiAbortHandle();
int APS5_VABI sceNpWebApiAddHttpRequestHeader();
int APS5_VABI sceNpWebApiAddMultipartPart();
int APS5_VABI sceNpWebApiCheckTimeout();
int APS5_VABI sceNpWebApiClearAllUnusedConnection();
int APS5_VABI sceNpWebApiClearUnusedConnection();
int APS5_VABI sceNpWebApiCreateMultipartRequest();
int APS5_VABI sceNpWebApiGetConnectionStats();
int APS5_VABI sceNpWebApiGetErrorCode();
int APS5_VABI sceNpWebApiInitializeForPresence();
int APS5_VABI sceNpWebApiIntCreateCtxIndExtdPushEventFilter();
int APS5_VABI sceNpWebApiIntCreateRequest();
int APS5_VABI sceNpWebApiIntCreateServicePushEventFilter();
int APS5_VABI sceNpWebApiIntInitialize();
int APS5_VABI sceNpWebApiIntRegisterServicePushEventCallback();
int APS5_VABI sceNpWebApiIntRegisterServicePushEventCallbackA();
int APS5_VABI sceNpWebApiRegisterExtdPushEventCallback();
int APS5_VABI sceNpWebApiRegisterNotificationCallback();
int APS5_VABI sceNpWebApiSendMultipartRequest();
int APS5_VABI sceNpWebApiSendMultipartRequest2();
int APS5_VABI sceNpWebApiSendRequest2();
int APS5_VABI sceNpWebApiSetHandleTimeout();
int APS5_VABI sceNpWebApiSetMaxConnection();
int APS5_VABI sceNpWebApiSetMultipartContentType();
int APS5_VABI sceNpWebApiSetRequestTimeout();
int APS5_VABI sceNpWebApiUnregisterNotificationCallback();
int APS5_VABI sceNpWebApiVshInitialize();

}
