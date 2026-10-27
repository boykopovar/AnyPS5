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
int APS5_VABI sceNpWebApiGetHttpStatusCode();
int APS5_VABI sceNpWebApiInitialize(int libHttpCtxId, std::size_t poolSize);
int APS5_VABI sceNpWebApiReadData();
int APS5_VABI sceNpWebApiRegisterPushEventCallback(int titleUserCtxId, int filterId, void* callback, void* userArg);
int APS5_VABI sceNpWebApiRegisterServicePushEventCallback(int titleUserCtxId, int filterId, void* callback, void* userArg);
int APS5_VABI sceNpWebApiSendRequest(std::int64_t requestId, const void* data, std::size_t dataSize);
int APS5_VABI sceNpWebApiTerminate(int libCtxId);
int APS5_VABI sceNpWebApiUnregisterPushEventCallback(int titleUserCtxId, int callbackId);
int APS5_VABI sceNpWebApiUnregisterServicePushEventCallback(int titleUserCtxId, int callbackId);
int APS5_VABI sceNpWebApiUtilityParseNpId();

}
