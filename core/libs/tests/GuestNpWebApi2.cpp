#include "prx/libSceNpWebApi2/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#ifdef __APPLE__
#include <initializer_list>
#endif
#include <cstring>
#include <stdexcept>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    constexpr int invalidContext = static_cast<int>(0x80553404);
    constexpr int invalidHandle = static_cast<int>(0x8055340D);
    const int context = sceNpWebApi2Initialize(1, 0x20000);
    const int other = sceNpWebApi2Initialize(1, 0x8000);
    Require(context > 0 && other > 0 && context != other);
    NpWebApi2MemoryPoolStats stats;
    std::memset(&stats, 0xa5, sizeof(stats));
    Require(sceNpWebApi2GetMemoryPoolStats(context, &stats) == 0);
    const auto* statsBytes = reinterpret_cast<const unsigned char*>(&stats);
    for (std::size_t index = sizeof(std::size_t); index < sizeof(stats); ++index) Require(statsBytes[index] == 0);
    Require(stats.pool_size == 0x20000 && stats.max_inuse_size == 0 && stats.current_inuse_size == 0 && stats.reserved == 0);
    Require(sceNpWebApi2GetMemoryPoolStats(other, &stats) == 0 && stats.pool_size == 0x8000);
    Require(sceNpWebApi2GetMemoryPoolStats(context, nullptr) == 0);
    Require(sceNpWebApi2GetMemoryPoolStats(-1, nullptr) == invalidContext);
    Require(sceNpWebApi2PushEventCreateHandle(-1) == invalidContext);
    const int handle = sceNpWebApi2PushEventCreateHandle(context);
    Require(handle > 0);
    Require(sceNpWebApi2PushEventAbortHandle(context, handle) == 0);
    Require(sceNpWebApi2PushEventAbortHandle(context, handle + 1000) == invalidHandle);
    Require(sceNpWebApi2PushEventSetHandleTimeout(context, handle, 30000) == 0);
    Require(sceNpWebApi2PushEventAbortHandle(other, handle) == invalidHandle);
    Require(sceNpWebApi2PushEventDeleteHandle(other, handle) == invalidHandle);
    Require(sceNpWebApi2PushEventSetHandleTimeout(other, handle, 0) == invalidHandle);
    Require(sceNpWebApi2PushEventSetHandleTimeout(context, handle, 0) == 0);
    Require(sceNpWebApi2PushEventSetHandleTimeout(context, handle, UINT32_MAX) == 0);
    Require(sceNpWebApi2PushEventDeleteHandle(context, handle) == 0);
    Require(sceNpWebApi2PushEventDeleteHandle(context, handle) == invalidHandle);
    Require(sceNpWebApi2PushEventAbortHandle(context, handle) == invalidHandle);
    Require(sceNpWebApi2PushEventSetHandleTimeout(context, handle, 1) == invalidHandle);
    const int survivingHandle = sceNpWebApi2PushEventCreateHandle(other);
    const int user = sceNpWebApi2CreateUserContext(context, 1);
    std::int64_t request = 0;
    Require(user > 0 && sceNpWebApi2CreateRequest(user, "sessionManager", "/v1/sessions", "GET", nullptr, &request) == 0);
    Require(sceNpWebApi2SetRequestTimeout(request, 30000000) == 0);
    Require(sceNpWebApi2AddWebTraceTag(request, "trace-tag-1") == 0);
    Require(sceNpWebApi2AddWebTraceTag(request, nullptr) == static_cast<int>(0x80553402));
    Require(sceNpWebApi2DeleteRequest(request) == 0);
    Require(sceNpWebApi2Terminate(context) == 0);
    Require(sceNpWebApi2GetMemoryPoolStats(context, &stats) == invalidContext);
    Require(sceNpWebApi2PushEventAbortHandle(context, handle) == invalidContext);
    Require(sceNpWebApi2PushEventSetHandleTimeout(context, handle, 30000) == invalidContext);
    Require(sceNpWebApi2PushEventAbortHandle(other, survivingHandle) == 0);
    Require(sceNpWebApi2Terminate(other) == 0);

    constexpr int invalidArgument = static_cast<int>(0x80553402);
    NpWebApi2IntInitializeArgs args{1, 0, 0x4000, "anyps5", sizeof(NpWebApi2IntInitializeArgs)};
    const int internal = sceNpWebApi2IntInitialize(&args);
    Require(internal > 0 && sceNpWebApi2GetMemoryPoolStats(internal, &stats) == 0 && stats.pool_size == 0x4000);
    args.struct_size = sizeof(NpWebApi2IntInitializeArgs) - 8;
    Require(sceNpWebApi2IntInitialize(&args) == invalidArgument && sceNpWebApi2IntInitialize(nullptr) == invalidArgument);
    NpWebApi2IntInitialize2Args args2{1, 0, 0x6000, nullptr, 1, 0, sizeof(NpWebApi2IntInitialize2Args)};
    const int internal2 = sceNpWebApi2IntInitialize2(&args2);
    Require(internal2 > 0 && internal2 != internal && sceNpWebApi2GetMemoryPoolStats(internal2, &stats) == 0 && stats.pool_size == 0x6000);
    args2.struct_size = sizeof(NpWebApi2IntInitializeArgs);
    Require(sceNpWebApi2IntInitialize2(&args2) == invalidArgument && sceNpWebApi2IntInitialize2(nullptr) == invalidArgument);
    const int presence = sceNpWebApi2InitializeForPresence(1, 0x2000);
    Require(presence > 0 && sceNpWebApi2GetMemoryPoolStats(presence, &stats) == 0 && stats.pool_size == 0x2000);
    constexpr int unavailable = static_cast<int>(0x80553406);
    const int multipartUser = sceNpWebApi2CreateUserContext(presence, 1);
    std::int64_t multipart = 0;
    Require(sceNpWebApi2CreateMultipartRequest(multipartUser, nullptr, "/v1/files", "POST", &multipart) == invalidArgument);
    Require(sceNpWebApi2CreateMultipartRequest(multipartUser, "fileManager", nullptr, "POST", &multipart) == invalidArgument);
    Require(sceNpWebApi2CreateMultipartRequest(multipartUser, "fileManager", "/v1/files", nullptr, &multipart) == invalidArgument);
    Require(multipart == 0);
    Require(sceNpWebApi2CreateMultipartRequest(multipartUser, "fileManager", "/v1/files", "POST", &multipart) == 0 && multipart > 0);
    std::int64_t nextMultipart = 0;
    Require(sceNpWebApi2CreateMultipartRequest(multipartUser, "fileManager", "/v1/files", "POST", &nextMultipart) == 0 && nextMultipart > multipart);
    Require(sceNpWebApi2CreateMultipartRequest(multipartUser, "fileManager", "/v1/files", "POST", nullptr) == 0);
    unsigned char part[4]{1, 2, 3, 4};
    Require(sceNpWebApi2SendMultipartRequest(multipart, 0, part, sizeof(part), nullptr) == invalidArgument);
    Require(sceNpWebApi2SendMultipartRequest(multipart, -1, part, sizeof(part), nullptr) == invalidArgument);
    Require(sceNpWebApi2SendMultipartRequest(multipart, 1, nullptr, sizeof(part), nullptr) == invalidArgument);
    Require(sceNpWebApi2SendMultipartRequest(multipart, 1, part, 0, nullptr) == invalidArgument);
    Require(sceNpWebApi2SendMultipartRequest(multipart, 1, part, sizeof(part), nullptr) == unavailable);
    Require(sceNpWebApi2DeleteRequest(multipart) == 0 && sceNpWebApi2DeleteRequest(nextMultipart) == 0);
    for (const int created : {internal, internal2, presence}) {
        Require(sceNpWebApi2Terminate(created) == 0);
        Require(sceNpWebApi2GetMemoryPoolStats(created, &stats) == invalidContext);
    }

    const int pushLibrary = sceNpWebApi2Initialize(1, 0x10000);
    const int pushUser = sceNpWebApi2CreateUserContext(pushLibrary, 1);
    const int otherUser = sceNpWebApi2CreateUserContext(pushLibrary, 2);
    Require(pushLibrary > 0 && pushUser > 0 && otherUser > 0 && otherUser != pushUser);
    std::int64_t sent = 0;
    Require(sceNpWebApi2CreateRequest(pushUser, "sessionManager", "/v1/sessions", "GET", nullptr, nullptr) == invalidArgument);
    Require(sceNpWebApi2CreateRequest(pushUser, "sessionManager", "/v1/sessions", "GET", nullptr, &sent) == 0 && sent > 0);
    Require(sceNpWebApi2AddHttpRequestHeader(sent, "Accept", "application/json") == 0);
    Require(sceNpWebApi2SendRequest(sent, nullptr, 0, nullptr) == unavailable);
    Require(sceNpWebApi2AbortRequest(sent) == 0);
    sceNpWebApi2CheckTimeout();
    Require(sceNpWebApi2DeleteRequest(sent) == 0);
    const int pushHandle = sceNpWebApi2PushEventCreateHandle(pushLibrary);
    const int filter = sceNpWebApi2PushEventCreateFilter(pushLibrary, pushHandle, "sessionManager", 0, nullptr, 0);
    const int nextFilter = sceNpWebApi2PushEventCreateFilter(pushLibrary, pushHandle, "sessionManager", 0, nullptr, 0);
    Require(pushHandle > 0 && filter > 0 && nextFilter > 0 && nextFilter != filter);
    int userArg = 0;
    const int callback = sceNpWebApi2PushEventRegisterCallback(pushUser, filter, nullptr, &userArg);
    const int contextCallback = sceNpWebApi2PushEventRegisterPushContextCallback(pushUser, nextFilter, nullptr, &userArg);
    Require(callback > 0 && contextCallback > 0 && contextCallback != callback);
    NpWebApi2PushContextId pushContext{};
    Require(sceNpWebApi2PushEventCreatePushContext(pushUser, &pushContext) == unavailable);
    Require(sceNpWebApi2PushEventStartPushContextCallback(pushUser, &pushContext) == 0);
    Require(sceNpWebApi2PushEventUnregisterPushContextCallback(pushUser, contextCallback) == 0);
    Require(sceNpWebApi2PushEventUnregisterCallback(pushUser, callback) == 0);
    Require(sceNpWebApi2PushEventDeletePushContext(pushUser, &pushContext) == 0);
    Require(sceNpWebApi2PushEventDeleteFilter(pushLibrary, nextFilter) == 0 && sceNpWebApi2PushEventDeleteFilter(pushLibrary, filter) == 0);
    Require(sceNpWebApi2PushEventDeleteHandle(pushLibrary, pushHandle) == 0);
    Require(sceNpWebApi2DeleteUserContext(otherUser) == 0 && sceNpWebApi2DeleteUserContext(pushUser) == 0);
    Require(sceNpWebApi2Terminate(pushLibrary) == 0);
    return 0;
}
