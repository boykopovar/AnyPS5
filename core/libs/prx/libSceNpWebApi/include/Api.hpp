#pragma once

#include "SceTypes.hpp"

struct NpWebApiIntInitializeArgs {
    std::uint32_t libHttpCtxId;
    std::uint8_t reserved[4];
    std::size_t poolSize;
    const char* name;
    std::size_t structSize;
};

static_assert(sizeof(NpWebApiIntInitializeArgs) == 0x20);

struct NpWebApiIntCreateRequestExtraArgs {
    const void* unknown0;
    const void* unknown1;
    const void* unknown2;
};

static_assert(sizeof(NpWebApiIntCreateRequestExtraArgs) == 24);

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

int APS5_VABI sceNpWebApiAbortHandle(int libCtxId, int handleId);
int APS5_VABI sceNpWebApiAddHttpRequestHeader(std::int64_t requestId, const char* fieldName, const char* value);
int APS5_VABI sceNpWebApiAddMultipartPart();
void APS5_VABI sceNpWebApiCheckTimeout();
int APS5_VABI sceNpWebApiClearAllUnusedConnection();
int APS5_VABI sceNpWebApiClearUnusedConnection();
int APS5_VABI sceNpWebApiCreateMultipartRequest(int titleUserCtxId, const char* apiGroup, const char* path, int method, std::int64_t* requestId);
int APS5_VABI sceNpWebApiGetConnectionStats();
int APS5_VABI sceNpWebApiGetErrorCode();
int APS5_VABI sceNpWebApiInitializeForPresence(int libHttpCtxId, std::size_t poolSize);
int APS5_VABI sceNpWebApiIntCreateCtxIndExtdPushEventFilter();
int APS5_VABI sceNpWebApiIntCreateRequest(int titleUserCtxId, const char* apiGroup, const char* path, int method, const NpWebApiContentParameter* contentParameter, const NpWebApiIntCreateRequestExtraArgs* internalArgs, std::int64_t* requestId);
int APS5_VABI sceNpWebApiIntCreateServicePushEventFilter();
int APS5_VABI sceNpWebApiIntInitialize(const NpWebApiIntInitializeArgs* args);
int APS5_VABI sceNpWebApiIntRegisterServicePushEventCallback();
int APS5_VABI sceNpWebApiIntRegisterServicePushEventCallbackA();
int APS5_VABI sceNpWebApiRegisterExtdPushEventCallback();
int APS5_VABI sceNpWebApiRegisterNotificationCallback(int titleUserCtxId, void* callback, void* userArg);
int APS5_VABI sceNpWebApiSendMultipartRequest();
int APS5_VABI sceNpWebApiSendMultipartRequest2();
int APS5_VABI sceNpWebApiSendRequest2(std::int64_t requestId, const void* data, std::size_t dataSize, void* responseInformation);
int APS5_VABI sceNpWebApiSetHandleTimeout(int libCtxId, int handleId, std::uint32_t timeout);
int APS5_VABI sceNpWebApiSetMaxConnection();
int APS5_VABI sceNpWebApiSetMultipartContentType(std::int64_t requestId, const char* typeName, const char* boundary);
int APS5_VABI sceNpWebApiSetRequestTimeout(std::int64_t requestId, std::uint32_t timeout);
int APS5_VABI sceNpWebApiUnregisterNotificationCallback(int titleUserCtxId);
int APS5_VABI sceNpWebApiVshInitialize(int libHttpCtxId, std::size_t poolSize);

}
