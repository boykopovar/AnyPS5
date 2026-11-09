#pragma once

#include "SceTypes.hpp"

struct NpWebApi2MemoryPoolStats {
    size_t pool_size;
    size_t max_inuse_size;
    size_t current_inuse_size;
    int32_t reserved;
};

struct NpWebApi2PushContextId {
    char value[37];
};

extern "C" {

int APS5_VABI sceNpWebApi2AbortRequest(int64_t requestId);
int APS5_VABI sceNpWebApi2AddHttpRequestHeader(int64_t requestId, const char* fieldName, const char* value);
int APS5_VABI sceNpWebApi2AddWebTraceTag(std::int64_t requestId, const char* value);
void APS5_VABI sceNpWebApi2CheckTimeout(void);
int APS5_VABI sceNpWebApi2CreateRequest(int userContextId, const char* apiGroup, const char* path, const char* method, const void* contentParameter, int64_t* requestId);
int APS5_VABI sceNpWebApi2CreateUserContext(int libCtxId, int userId);
int APS5_VABI sceNpWebApi2DeleteRequest(int64_t requestId);
int APS5_VABI sceNpWebApi2DeleteUserContext(int userContextId);
int APS5_VABI sceNpWebApi2GetHttpResponseHeaderValue(int64_t requestId, const char* fieldName, char* value, size_t valueSize);
int APS5_VABI sceNpWebApi2GetHttpResponseHeaderValueLength(int64_t requestId, const char* fieldName, size_t* valueLength);
int APS5_VABI sceNpWebApi2Initialize(int libHttpCtxId, size_t poolSize);
int APS5_VABI sceNpWebApi2GetMemoryPoolStats(int libraryContextId, NpWebApi2MemoryPoolStats* stats);
int APS5_VABI sceNpWebApi2PushEventCreateFilter(int libCtxId, int handleId, const char* npServiceName, uint32_t npServiceLabel, const void* filterParam, size_t filterParamNum);
int APS5_VABI sceNpWebApi2PushEventCreateHandle(int libraryContextId);
int APS5_VABI sceNpWebApi2PushEventAbortHandle(int libraryContextId, int handleId);
int APS5_VABI sceNpWebApi2PushEventDeleteHandle(int libraryContextId, int handleId);
int APS5_VABI sceNpWebApi2PushEventDeletePushContext(int userContextId, const void* pushContextId);
int APS5_VABI sceNpWebApi2PushEventRegisterCallback(int userContextId, int filterId, void* callback, void* userArg);
int APS5_VABI sceNpWebApi2ReadData(int64_t requestId, void* data, size_t size);
int APS5_VABI sceNpWebApi2SendRequest(int64_t requestId, const void* data, size_t dataSize, NpWebApi2ResponseInformationOption* responseInfoOption);
int APS5_VABI sceNpWebApi2Terminate(int libCtxId);
int APS5_VABI sceNpWebApi2PushEventCreatePushContext(std::int32_t userContextId, NpWebApi2PushContextId* pushContextId);
int APS5_VABI sceNpWebApi2PushEventDeleteFilter(std::int32_t libraryContextId, std::int32_t filterId);
int APS5_VABI sceNpWebApi2PushEventSetHandleTimeout(int libraryContextId, int handleId, std::uint32_t timeout);
int APS5_VABI sceNpWebApi2PushEventRegisterPushContextCallback(std::int32_t userContextId, std::int32_t filterId, void* callback, void* userArg);
int APS5_VABI sceNpWebApi2PushEventStartPushContextCallback(std::int32_t userContextId, const NpWebApi2PushContextId* pushContextId);
int APS5_VABI sceNpWebApi2PushEventUnregisterCallback(std::int32_t userContextId, std::int32_t callbackId);
int APS5_VABI sceNpWebApi2PushEventUnregisterPushContextCallback(std::int32_t userContextId, std::int32_t callbackId);
int APS5_VABI sceNpWebApi2SetRequestTimeout(int64_t requestId, uint32_t timeout);
int APS5_VABI sceNpWebApi2AddMultipartPart();
int APS5_VABI sceNpWebApi2CreateMultipartRequest(int userCtxId, const char* apiGroup, const char* path, const char* method, int64_t* requestId);
int APS5_VABI sceNpWebApi2InitializeForPresence(int libHttpCtxId, uint64_t poolSize);
int APS5_VABI sceNpWebApi2IntCreateRequest();
int APS5_VABI sceNpWebApi2IntInitialize(const NpWebApi2IntInitializeArgs* args);
int APS5_VABI sceNpWebApi2IntInitialize2(const NpWebApi2IntInitialize2Args* args);
int APS5_VABI sceNpWebApi2IntPushEventCreateCtxIndFilter();
int APS5_VABI sceNpWebApi2IntPushEventCreateDestPs4CtxIndFilter();
int APS5_VABI sceNpWebApi2SendMultipartRequest(int64_t requestId, int partIndex, void* data, uint64_t dataSize, void* respInfoOption);
int APS5_VABI sceNpWebApi2SetMultipartContentType();

}
