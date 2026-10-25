#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceNpWebApiAbortRequest();
int APS5_VABI sceNpWebApiCreateContext();
int APS5_VABI sceNpWebApiCreateHandle(int libCtxId);
int APS5_VABI sceNpWebApiCreatePushEventFilter(int libCtxId, const void* filterParam, std::size_t filterParamNum);
int APS5_VABI sceNpWebApiCreateRequest();
int APS5_VABI sceNpWebApiCreateServicePushEventFilter(int libCtxId, int handleId, const char* npServiceName, std::uint32_t npServiceLabel, const void* filterParam, std::size_t filterParamNum);
int APS5_VABI sceNpWebApiDeleteContext();
int APS5_VABI sceNpWebApiDeleteHandle(int libCtxId, int handleId);
int APS5_VABI sceNpWebApiDeletePushEventFilter(int libCtxId, int filterId);
int APS5_VABI sceNpWebApiDeleteRequest();
int APS5_VABI sceNpWebApiDeleteServicePushEventFilter(int libCtxId, int filterId);
int APS5_VABI sceNpWebApiGetHttpResponseHeaderValue();
int APS5_VABI sceNpWebApiGetHttpResponseHeaderValueLength();
int APS5_VABI sceNpWebApiGetHttpStatusCode();
int APS5_VABI sceNpWebApiInitialize(int libHttpCtxId, std::size_t poolSize);
int APS5_VABI sceNpWebApiReadData();
int APS5_VABI sceNpWebApiRegisterPushEventCallback();
int APS5_VABI sceNpWebApiRegisterServicePushEventCallback();
int APS5_VABI sceNpWebApiSendRequest();
int APS5_VABI sceNpWebApiTerminate(int libCtxId);
int APS5_VABI sceNpWebApiUnregisterPushEventCallback();
int APS5_VABI sceNpWebApiUnregisterServicePushEventCallback();
int APS5_VABI sceNpWebApiUtilityParseNpId();

}
