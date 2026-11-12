#include "ProcessParametersFixture.hpp"
#include "prx/libSceNpWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <cstring>
#include <initializer_list>
#include <thread>

namespace {

constexpr int InvalidArgument = static_cast<int>(0x80552902u);
constexpr int InvalidLibContextId = static_cast<int>(0x80552903u);
constexpr int LibContextNotFound = static_cast<int>(0x80552904u);
constexpr int UserContextNotFound = static_cast<int>(0x80552905u);
constexpr int RequestNotFound = static_cast<int>(0x80552906u);
constexpr int NotSignedIn = static_cast<int>(0x80552907u);
constexpr int InvalidContentParameter = static_cast<int>(0x80552908u);
constexpr int Aborted = static_cast<int>(0x80552909u);
constexpr int UserContextAlreadyExist = static_cast<int>(0x8055290Au);
constexpr int PushEventFilterNotFound = static_cast<int>(0x8055290Bu);
constexpr int PushEventCallbackNotFound = static_cast<int>(0x8055290Cu);
constexpr int HandleNotFound = static_cast<int>(0x8055290Du);
constexpr int ServicePushEventFilterNotFound = static_cast<int>(0x8055290Eu);
constexpr int ServicePushEventCallbackNotFound = static_cast<int>(0x8055290Fu);
constexpr int SignedInUserNotFound = static_cast<int>(0x80552910u);
constexpr int ExtdPushEventFilterNotFound = static_cast<int>(0x8055291Cu);
constexpr int ExtdPushEventCallbackNotFound = static_cast<int>(0x8055291Du);
constexpr int NpUserNotFound = static_cast<int>(0x80960105u);
constexpr int InitialUser = 0x10000000;
constexpr int HttpInvalidId = static_cast<int>(0x80431100u);
constexpr int HttpInvalidResponse = static_cast<int>(0x80432060u);
constexpr int NpInvalidArgument = static_cast<int>(0x80550003u);

constexpr int MethodGet = 0;
constexpr int MethodPost = 1;
constexpr int MethodPatch = 4;

alignas(16) std::array<std::byte, 0x40> process{};
const char onlineId[20] = "player";
const char filterParam[16] = {};
const char body[] = "{}";

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "NpWebApi: %s\n", message);
        std::abort();
    }
}

void APS5_VABI PushEvent() {
}

void* PushEventCallback() {
    return reinterpret_cast<void*>(&PushEvent);
}

void SetSdkVersion(std::uint32_t version) {
    std::memcpy(process.data() + 16, &version, sizeof(version));
}

void InstallProcessParameters() {
    const std::uint64_t size = process.size();
    const std::uint32_t magic = 0x4942524f;
    std::memcpy(process.data(), &size, sizeof(size));
    std::memcpy(process.data() + 8, &magic, sizeof(magic));
    Require(ProcessParametersFixture::Install(process.data(), process.size()), "process parameters were not installed");
}

void TestUnknownFieldChecks() {
    struct Object {
        std::uint64_t words[6];
    };
    Object object{};
    for (const std::int64_t value : {std::int64_t{-1}, std::int64_t{0}, std::int64_t{1}, std::int64_t{-2}, INT64_MIN, INT64_MAX}) {
        std::memset(&object, 0xa5, sizeof(object));
        std::memcpy(reinterpret_cast<std::byte*>(&object) + 0x20, &value, sizeof(value));
        Require(libSceNpWebApiUnknown_ojGP5vur_plus_qM_nid_no_patch(&object) == (value != -1), "ojGP5vur+qM checks the 64 bits at +0x20");
    }
    for (const std::int32_t value : {std::int32_t{-1}, std::int32_t{0}, std::int32_t{1}, std::int32_t{-2}, INT32_MIN, INT32_MAX}) {
        std::memset(&object, 0xa5, sizeof(object));
        std::memcpy(reinterpret_cast<std::byte*>(&object) + 0xc, &value, sizeof(value));
        Require(libSceNpWebApiUnknown_zQE2rxZdLy8_nid_no_patch(&object) == (value != -1), "zQE2rxZdLy8 checks the 32 bits at +0xc");
    }
    std::memset(&object, 0xff, sizeof(object));
    const std::int32_t low = -1;
    const std::int32_t high = 0;
    std::memcpy(reinterpret_cast<std::byte*>(&object) + 0x20, &low, sizeof(low));
    std::memcpy(reinterpret_cast<std::byte*>(&object) + 0x24, &high, sizeof(high));
    Require(libSceNpWebApiUnknown_ojGP5vur_plus_qM_nid_no_patch(&object), "ojGP5vur+qM compares all 64 bits");
}

void TestLibraryContexts() {
    const int context = sceNpWebApiInitialize(1, 0x10000);
    Require(context > 0 && context <= 0x7fff, "library context id out of range");
    const int other = sceNpWebApiInitialize(1, 0x10000);
    Require(other > 0 && other != context, "a second library context must get its own id");
    Require(sceNpWebApiCreateHandle(0x7fff + 1) == LibContextNotFound, "handle of an unknown context");

    const int handle = sceNpWebApiCreateHandle(context);
    Require(handle > 0 && static_cast<unsigned>(handle) < 0xf0000000u, "handle id out of range");
    Require(sceNpWebApiCreatePushEventFilter(context, nullptr, 1) == InvalidArgument, "push event filter without its parameter");
    Require(sceNpWebApiCreatePushEventFilter(context, filterParam, 0) == InvalidArgument, "push event filter with no parameters");
    const int filter = sceNpWebApiCreatePushEventFilter(context, filterParam, 1);
    Require(filter > 0 && filter != handle, "push event filter id");

    Require(sceNpWebApiCreateServicePushEventFilter(context, handle, nullptr, 0, filterParam, 1) == InvalidArgument, "service filter without a service name");
    Require(sceNpWebApiCreateServicePushEventFilter(0x7fff + 1, handle, "service", 0, filterParam, 1) == LibContextNotFound, "service filter of an unknown context");
    Require(sceNpWebApiCreateServicePushEventFilter(other, handle, "service", 0, filterParam, 1) == HandleNotFound, "service filter with another context's handle");
    Require(sceNpWebApiCreateServicePushEventFilter(context, handle, "service", 0xffffffffu, filterParam, 1) == InvalidArgument, "service filter with the invalid service label from SDK 2.00");
    Require(sceNpWebApiCreateServicePushEventFilter(0x7fff + 1, handle, "service", 0xffffffffu, filterParam, 1) == InvalidArgument, "the service label is checked before the library context");
    Require(sceNpWebApiCreateServicePushEventFilter(context, handle, "service", 0, filterParam, 1) == SignedInUserNotFound, "service filter without a signed-in user");
    Require(sceNpWebApiDeleteServicePushEventFilter(context, 1) == ServicePushEventFilterNotFound, "no service filter exists");

    Require(sceNpWebApiDeletePushEventFilter(other, filter) == PushEventFilterNotFound, "filter deleted through another context");
    Require(sceNpWebApiDeletePushEventFilter(context, filter) == 0, "delete the push event filter");
    Require(sceNpWebApiDeletePushEventFilter(context, filter) == PushEventFilterNotFound, "push event filter deleted twice");
    Require(sceNpWebApiDeleteHandle(context, handle) == 0, "delete the handle");
    Require(sceNpWebApiDeleteHandle(context, handle) == HandleNotFound, "handle deleted twice");

    Require(sceNpWebApiTerminate(context) == 0, "terminate the library context");
    Require(sceNpWebApiTerminate(context) == LibContextNotFound, "library context terminated twice");
    Require(sceNpWebApiCreateHandle(context) == LibContextNotFound, "handle of a terminated context");
    Require(sceNpWebApiTerminate(other) == 0, "terminate the second library context");
}

void TestIntServicePushEventFilter() {
    const int context = sceNpWebApiInitialize(1, 0x10000);
    const int other = sceNpWebApiInitialize(1, 0x10000);
    Require(context > 0 && other > 0, "library contexts for int service filter");
    const int handle = sceNpWebApiCreateHandle(context);
    Require(handle > 0, "handle for int service filter");

    Require(sceNpWebApiIntCreateServicePushEventFilter(context, handle, "service", 0, nullptr, 1) == InvalidArgument, "int service filter without filter parameter");
    Require(sceNpWebApiIntCreateServicePushEventFilter(context, handle, "service", 0, filterParam, 0) == InvalidArgument, "int service filter with count 0");
    Require(sceNpWebApiIntCreateServicePushEventFilter(0x7fff + 1, handle, "service", 0, filterParam, 1) == LibContextNotFound, "int service filter of an unknown context");
    Require(sceNpWebApiIntCreateServicePushEventFilter(other, handle, "service", 0, filterParam, 1) == HandleNotFound, "int service filter with another context's handle");
    Require(sceNpWebApiIntCreateServicePushEventFilter(context, handle, "service", 0xffffffffu, filterParam, 1) == SignedInUserNotFound, "int service filter with invalid service label");
    Require(sceNpWebApiIntCreateServicePushEventFilter(context, handle, "service", 0, filterParam, 1) == SignedInUserNotFound, "int service filter without signed-in user");

    Require(sceNpWebApiDeleteHandle(context, handle) == 0, "delete the handle for int service filter");
    Require(sceNpWebApiTerminate(context) == 0 && sceNpWebApiTerminate(other) == 0, "terminate the int service filter library contexts");
}

void TestUserContexts() {
    const int context = sceNpWebApiInitialize(1, 0x10000);
    const int other = sceNpWebApiInitialize(1, 0x10000);
    Require(context > 0 && other > 0, "library contexts for user contexts");
    Require(sceNpWebApiTerminate(other) == 0, "terminate the unused library context");

    Require(sceNpWebApiCreateContext(0x8000, onlineId) == InvalidLibContextId, "user context of a library context id above the range");
    Require(sceNpWebApiCreateContext(0x8000, nullptr) == InvalidLibContextId, "the library context id is checked before the online id");
    Require(sceNpWebApiCreateContext(context, nullptr) == InvalidArgument, "user context without an online id");
    Require(sceNpWebApiCreateContext(0, onlineId) == LibContextNotFound, "user context of library context 0");
    Require(sceNpWebApiCreateContext(other, onlineId) == LibContextNotFound, "user context of a terminated library context");

    const int user = sceNpWebApiCreateContext(context, onlineId);
    Require(user > 0 && (user >> 16) == context && (user & 0xffff) != 0, "user context id holds its library context");
    Require(sceNpWebApiCreateContext(context, onlineId) == UserContextAlreadyExist, "a second user context for the same user");

    Require(sceNpWebApiDeleteContext(user + 1) == UserContextNotFound, "delete an unknown user context");
    Require(sceNpWebApiDeleteContext(-1) == LibContextNotFound, "delete user context -1");
    Require(sceNpWebApiDeleteContext((other << 16) | 1) == LibContextNotFound, "delete a user context of a terminated library context");
    Require(sceNpWebApiDeleteContext(user) == 0, "delete the user context");
    Require(sceNpWebApiDeleteContext(user) == UserContextNotFound, "user context deleted twice");

    const int next = sceNpWebApiCreateContext(context, onlineId);
    Require(next > 0 && next != user && (next >> 16) == context, "a new user context after the first was deleted");
    std::int64_t request = 0;
    Require(sceNpWebApiCreateRequest(next, "group", "/path", MethodGet, nullptr, &request) == 0, "request before termination");
    Require(sceNpWebApiTerminate(context) == 0, "terminate a library context with a user context");
    Require(sceNpWebApiDeleteContext(next) == LibContextNotFound, "user context of a terminated library context");
    Require(sceNpWebApiSendRequest(request, nullptr, 0) == LibContextNotFound, "request of a terminated library context");
}

void TestRequests() {
    const int context = sceNpWebApiInitialize(1, 0x10000);
    const int user = sceNpWebApiCreateContext(context, onlineId);
    Require(user > 0, "user context for requests");

    std::int64_t request = 0;
    Require(sceNpWebApiCreateRequest(user, nullptr, "/path", MethodGet, nullptr, &request) == InvalidArgument, "request without an API group");
    Require(sceNpWebApiCreateRequest(user, "group", nullptr, MethodGet, nullptr, &request) == InvalidArgument, "request without a path");
    const NpWebApiContentParameter untyped{sizeof(body), nullptr, {}};
    Require(sceNpWebApiCreateRequest(user, "group", "/path", MethodPost, &untyped, &request) == InvalidContentParameter, "content without a type");
    Require(sceNpWebApiCreateRequest(0, "group", "/path", MethodPost, &untyped, &request) == InvalidContentParameter, "content is checked before the user context");
    Require(sceNpWebApiCreateRequest(user, "group", "/path", MethodPatch, nullptr, &request) == InvalidArgument, "PATCH with SDK 2.50 or later");
    SetSdkVersion(0x05050001);
    request = 0;
    Require(sceNpWebApiCreateRequest(user, "userProfile", "/v1/users/me/profile", 99, nullptr, &request) == InvalidArgument && request == 0, "method 99 at SDK 0x05050001");
    SetSdkVersion(0x04508001);
    Require(sceNpWebApiCreateRequest(user + 1, "group", "/path", MethodGet, nullptr, &request) == UserContextNotFound, "request of an unknown user context");
    Require(sceNpWebApiCreateRequest(0, "group", "/path", MethodGet, nullptr, &request) == LibContextNotFound, "request of user context 0");
    Require(sceNpWebApiCreateRequest(-1, "group", "/path", MethodGet, nullptr, &request) == LibContextNotFound, "request of user context -1");
    Require(request == 0, "a failed request leaves the id alone");

    const NpWebApiContentParameter empty{0, nullptr, {}};
    Require(sceNpWebApiCreateRequest(user, "group", "/path", MethodGet, &empty, nullptr) == 0, "empty content without a type and no id output");
    const NpWebApiContentParameter typed{sizeof(body), "application/json", {}};
    Require(sceNpWebApiCreateRequest(user, "group", "/path", MethodPost, &typed, &request) == 0, "create a request");
    Require(static_cast<int>(request >> 32) == user && (request & 0xffffffff) != 0, "request id holds its user context");
    std::int64_t second = 0;
    Require(sceNpWebApiCreateRequest(user, "group", "/path", MethodGet, nullptr, &second) == 0 && second != request && static_cast<int>(second >> 32) == user, "a second request");

    Require(sceNpWebApiSendRequest(request, body, sizeof(body)) == NotSignedIn, "a request cannot be sent without a signed-in user");
    Require(sceNpWebApiSendRequest(request, body, sizeof(body)) == NotSignedIn, "a failed send can be retried");
    Require(sceNpWebApiSendRequest(request + 0x10000, nullptr, 0) == RequestNotFound, "send an unknown request");
    Require(sceNpWebApiSendRequest(static_cast<std::int64_t>(user + 1) << 32 | 1, nullptr, 0) == UserContextNotFound, "send a request of an unknown user context");
    Require(sceNpWebApiSendRequest(1, nullptr, 0) == LibContextNotFound, "send a request of library context 0");

    Require(sceNpWebApiAbortRequest(request + 0x10000) == RequestNotFound, "abort an unknown request");
    Require(sceNpWebApiAbortRequest(-1) == LibContextNotFound, "abort request -1");
    Require(sceNpWebApiDeleteRequest(-1) == LibContextNotFound, "delete request -1");
    Require(sceNpWebApiAbortRequest(request) == 0, "abort the request");
    Require(sceNpWebApiAbortRequest(request) == 0, "abort the request twice");
    Require(sceNpWebApiSendRequest(request, body, sizeof(body)) == Aborted, "send an aborted request");
    Require(sceNpWebApiSendRequest(second, nullptr, 0) == NotSignedIn, "aborting one request leaves the other");

    Require(sceNpWebApiDeleteRequest(request) == 0, "delete the request");
    Require(sceNpWebApiDeleteRequest(request) == RequestNotFound, "request deleted twice");
    Require(sceNpWebApiSendRequest(request, nullptr, 0) == RequestNotFound, "send a deleted request");
    Require(sceNpWebApiDeleteContext(user) == 0, "delete the user context with a request");
    Require(sceNpWebApiSendRequest(second, nullptr, 0) == UserContextNotFound, "request of a deleted user context");
    Require(sceNpWebApiDeleteRequest(second) == UserContextNotFound, "delete a request of a deleted user context");
    Require(sceNpWebApiTerminate(context) == 0, "terminate the request library context");
}

void TestResponses() {
    const int context = sceNpWebApiInitialize(1, 0x10000);
    const int user = sceNpWebApiCreateContext(context, onlineId);
    Require(user > 0, "user context for responses");
    std::int64_t request = 0;
    std::int64_t aborted = 0;
    Require(sceNpWebApiCreateRequest(user, "group", "/path", MethodGet, nullptr, &request) == 0, "request for responses");
    Require(sceNpWebApiCreateRequest(user, "group", "/path", MethodGet, nullptr, &aborted) == 0, "request to abort");
    Require(sceNpWebApiSendRequest(request, nullptr, 0) == NotSignedIn, "the request is never sent");
    Require(sceNpWebApiAbortRequest(aborted) == 0, "abort the second request");

    std::array<char, 8> data{};
    data.fill('x');
    Require(sceNpWebApiReadData(request, nullptr, data.size()) == InvalidArgument, "read without a buffer");
    Require(sceNpWebApiReadData(request, data.data(), 0) == InvalidArgument, "read of zero bytes");
    Require(sceNpWebApiReadData(-1, nullptr, 0) == InvalidArgument, "the buffer is checked before the request");
    Require(sceNpWebApiReadData(-1, data.data(), data.size()) == LibContextNotFound, "read request -1");
    Require(sceNpWebApiReadData(static_cast<std::int64_t>(user + 1) << 32 | 1, data.data(), data.size()) == UserContextNotFound, "read a request of an unknown user context");
    Require(sceNpWebApiReadData(request + 0x10000, data.data(), data.size()) == RequestNotFound, "read an unknown request");
    Require(sceNpWebApiReadData(request, data.data(), data.size()) == HttpInvalidId, "read a request that was never sent");
    Require(sceNpWebApiReadData(aborted, data.data(), data.size()) == Aborted, "read an aborted request");
    Require(data[0] == 'x' && data[7] == 'x', "a failed read leaves the buffer alone");

    std::array<char, 8> value{};
    value.fill('x');
    std::size_t length = 123;
    Require(sceNpWebApiGetHttpResponseHeaderValue(request, nullptr, value.data(), value.size()) == InvalidArgument, "header value without a field name");
    Require(sceNpWebApiGetHttpResponseHeaderValue(request, "Content-Type", nullptr, value.size()) == InvalidArgument, "header value without a buffer");
    Require(sceNpWebApiGetHttpResponseHeaderValue(request, "Content-Type", value.data(), 0) == InvalidArgument, "header value with an empty buffer");
    Require(sceNpWebApiGetHttpResponseHeaderValueLength(request, nullptr, &length) == InvalidArgument, "header length without a field name");
    Require(sceNpWebApiGetHttpResponseHeaderValueLength(request, "Content-Type", nullptr) == InvalidArgument, "header length without an output");
    Require(sceNpWebApiGetHttpResponseHeaderValue(-1, "Content-Type", value.data(), value.size()) == LibContextNotFound, "header value of request -1");
    Require(sceNpWebApiGetHttpResponseHeaderValueLength(request + 0x10000, "Content-Type", &length) == RequestNotFound, "header length of an unknown request");
    Require(sceNpWebApiGetHttpResponseHeaderValue(request, "Content-Type", value.data(), value.size()) == HttpInvalidResponse, "header value of a request that was never sent");
    Require(sceNpWebApiGetHttpResponseHeaderValueLength(request, "Content-Type", &length) == HttpInvalidResponse, "header length of a request that was never sent");
    Require(sceNpWebApiGetHttpResponseHeaderValueLength(aborted, "Content-Type", &length) == HttpInvalidResponse, "header length of an aborted request");
    Require(value[0] == 'x' && value[7] == 'x' && length == 123, "a failed header query leaves the outputs alone");

    int statusCode = 999;
    Require(sceNpWebApiGetHttpStatusCode(request, nullptr) == InvalidArgument, "status code without an output");
    Require(sceNpWebApiGetHttpStatusCode(-1, nullptr) == InvalidArgument, "the output pointer is checked before the request");
    Require(sceNpWebApiGetHttpStatusCode(-1, &statusCode) == LibContextNotFound, "status code of request -1");
    Require(sceNpWebApiGetHttpStatusCode(static_cast<std::int64_t>(user + 1) << 32 | 1, &statusCode) == UserContextNotFound, "status code of a request of an unknown user context");
    Require(sceNpWebApiGetHttpStatusCode(request + 0x10000, &statusCode) == RequestNotFound, "status code of an unknown request");
    Require(sceNpWebApiGetHttpStatusCode(request, &statusCode) == HttpInvalidResponse, "status code of a request that was never sent");
    Require(sceNpWebApiGetHttpStatusCode(aborted, &statusCode) == HttpInvalidResponse, "status code of an aborted request");
    Require(statusCode == 999, "a failed status code query leaves the output alone");

    Require(sceNpWebApiDeleteRequest(request) == 0 && sceNpWebApiDeleteRequest(aborted) == 0, "delete the response requests");
    Require(sceNpWebApiGetHttpStatusCode(request, &statusCode) == RequestNotFound, "status code of a deleted request");
    Require(sceNpWebApiReadData(request, data.data(), data.size()) == RequestNotFound, "read a deleted request");
    Require(sceNpWebApiTerminate(context) == 0, "terminate the response library context");
}

void TestPushEventCallbacks() {
    const int context = sceNpWebApiInitialize(1, 0x10000);
    const int other = sceNpWebApiInitialize(1, 0x10000);
    const int user = sceNpWebApiCreateContext(context, onlineId);
    Require(user > 0, "user context for callbacks");
    const int filter = sceNpWebApiCreatePushEventFilter(context, filterParam, 1);
    const int otherFilter = sceNpWebApiCreatePushEventFilter(other, filterParam, 1);
    Require(filter > 0 && otherFilter > 0, "push event filters for callbacks");
    int argument = 0;

    Require(sceNpWebApiRegisterPushEventCallback(user, filter, nullptr, &argument) == InvalidArgument, "push event callback without a function");
    Require(sceNpWebApiRegisterPushEventCallback(user + 1, filter, nullptr, &argument) == InvalidArgument, "the function is checked before the user context");
    Require(sceNpWebApiRegisterPushEventCallback(user + 1, filter, PushEventCallback(), &argument) == UserContextNotFound, "push event callback of an unknown user context");
    Require(sceNpWebApiRegisterPushEventCallback(user, otherFilter, PushEventCallback(), &argument) == PushEventFilterNotFound, "push event callback with another context's filter");
    const int callback = sceNpWebApiRegisterPushEventCallback(user, filter, PushEventCallback(), &argument);
    Require(callback > 0 && static_cast<unsigned>(callback) < 0xf0000000u, "push event callback id");
    Require(sceNpWebApiUnregisterPushEventCallback(user + 1, callback) == UserContextNotFound, "unregister through an unknown user context");
    Require(sceNpWebApiUnregisterServicePushEventCallback(user, callback) == ServicePushEventCallbackNotFound, "a push event callback is not a service callback");
    Require(sceNpWebApiUnregisterPushEventCallback(user, callback) == 0, "unregister the push event callback");
    Require(sceNpWebApiUnregisterPushEventCallback(user, callback) == PushEventCallbackNotFound, "push event callback unregistered twice");

    Require(sceNpWebApiRegisterServicePushEventCallback(user, 1, nullptr, &argument) == InvalidArgument, "service callback without a function");
    Require(sceNpWebApiRegisterServicePushEventCallback(user + 1, 1, PushEventCallback(), &argument) == UserContextNotFound, "service callback of an unknown user context");
    Require(sceNpWebApiRegisterServicePushEventCallback(user, filter, PushEventCallback(), &argument) == ServicePushEventFilterNotFound, "a push event filter is not a service filter");
    Require(sceNpWebApiUnregisterServicePushEventCallback(user, 1) == ServicePushEventCallbackNotFound, "no service callback exists");

    Require(sceNpWebApiRegisterExtdPushEventCallback(user, filter, nullptr, &argument) == InvalidArgument, "extended push event callback without a function");
    Require(sceNpWebApiRegisterExtdPushEventCallback(user + 1, filter, nullptr, &argument) == InvalidArgument, "the function is checked before the user context for extended callbacks");
    Require(sceNpWebApiRegisterExtdPushEventCallback(user + 1, filter, PushEventCallback(), &argument) == UserContextNotFound, "extended push event callback of an unknown user context");
    Require(sceNpWebApiRegisterExtdPushEventCallback(user, 1, PushEventCallback(), &argument) == ExtdPushEventFilterNotFound, "extended push event filter does not exist");

    Require(sceNpWebApiIntRegisterServicePushEventCallback(user, 1, nullptr, &argument) == InvalidArgument, "int service callback without a function");
    Require(sceNpWebApiIntRegisterServicePushEventCallback(user + 1, 1, PushEventCallback(), &argument) == UserContextNotFound, "int service callback of an unknown user context");
    Require(sceNpWebApiIntRegisterServicePushEventCallback(user, filter, PushEventCallback(), &argument) == ServicePushEventFilterNotFound, "int service callback with a push event filter");

    Require(sceNpWebApiIntRegisterServicePushEventCallbackA(user, 1, nullptr, &argument) == InvalidArgument, "int service callback A without a function");
    Require(sceNpWebApiIntRegisterServicePushEventCallbackA(user + 1, 1, PushEventCallback(), &argument) == UserContextNotFound, "int service callback A of an unknown user context");
    Require(sceNpWebApiIntRegisterServicePushEventCallbackA(user, filter, PushEventCallback(), &argument) == ServicePushEventFilterNotFound, "int service callback A with a push event filter");

    Require(sceNpWebApiTerminate(context) == 0 && sceNpWebApiTerminate(other) == 0, "terminate the callback library contexts");
}

void TestConsoleMeasuredExports() {
    const int context = sceNpWebApiInitialize(1, 0x10000);
    Require(context > 0, "library context for the console-measured exports");
    NpWebApiMemoryPoolStats stats{1, 1, 1};
    Require(sceNpWebApiGetMemoryPoolStats(context, &stats) == 0 && stats.poolSize == 0x10000 && stats.maxInuseSize == 0 && stats.currentInuseSize == 0, "memory pool stats of a fresh context");
    Require(sceNpWebApiGetMemoryPoolStats(0x7fff + 1, &stats) == LibContextNotFound, "memory pool stats of an unknown context");
    Require(sceNpWebApiGetMemoryPoolStats(context, nullptr) == InvalidArgument, "memory pool stats without a buffer");
    errno = 71;
    Require(sceNpWebApiGetMemoryPoolStats(context, reinterpret_cast<NpWebApiMemoryPoolStats*>(0x10)) == InvalidArgument && errno == 71, "memory pool stats with an unwritable buffer");

    Require(sceNpWebApiCreateContextA(context, -1) == InvalidArgument, "user context for a negative user");
    Require(sceNpWebApiCreateContextA(0x7fff + 1, InitialUser) == InvalidLibContextId, "user context of an unknown library context by user id");
    Require(sceNpWebApiCreateContextA(context, 0) == NpUserNotFound, "user context for a user that is not logged in");
    const int user = sceNpWebApiCreateContextA(context, InitialUser);
    Require(user > 0 && (user >> 16) == context && (user & 0xffff) != 0, "user context by user id holds its library context");
    Require(sceNpWebApiCreateContextA(context, InitialUser) == UserContextAlreadyExist, "second user context for the same user id");
    Require(sceNpWebApiCreateContext(context, onlineId) == UserContextAlreadyExist, "the online id variant shares the user context");

    const int handle = sceNpWebApiCreateHandle(context);
    Require(handle > 0, "handle for the extended filters");
    Require(sceNpWebApiCreateExtdPushEventFilter(context, handle, "service", 0, filterParam, 0) == InvalidArgument, "extended filter with no parameters");
    Require(sceNpWebApiCreateExtdPushEventFilter(context, -1, "service", 0, filterParam, 1) == HandleNotFound, "extended filter with an unknown handle");
    Require(sceNpWebApiCreateExtdPushEventFilter(0x7fff + 1, handle, "service", 0, filterParam, 1) == LibContextNotFound, "extended filter of an unknown context");
    Require(sceNpWebApiCreateExtdPushEventFilter(context, handle, "service", 0, filterParam, 1) == SignedInUserNotFound, "extended filter without a signed-in user");
    int argument = 0;
    Require(sceNpWebApiRegisterExtdPushEventCallbackA(user, 1, PushEventCallback(), &argument) == ExtdPushEventFilterNotFound, "extended callback on a filter that does not exist");
    Require(sceNpWebApiRegisterExtdPushEventCallbackA(-1, 1, PushEventCallback(), &argument) == LibContextNotFound, "extended callback through an unknown user context");
    Require(sceNpWebApiUnregisterExtdPushEventCallback(user, 1) == ExtdPushEventCallbackNotFound, "unregister an extended callback that does not exist");
    Require(sceNpWebApiDeleteExtdPushEventFilter(context, 1) == ExtdPushEventFilterNotFound, "delete an extended filter that does not exist");
    Require(sceNpWebApiDeleteExtdPushEventFilter(0x7fff + 1, 1) == LibContextNotFound, "delete an extended filter of an unknown context");

    Require(sceNpWebApiDeleteContext(user) == 0, "delete the user context made by user id");
    Require(sceNpWebApiDeleteContext(user) == UserContextNotFound, "user context made by user id deleted twice");
    Require(sceNpWebApiRegisterExtdPushEventCallbackA(user, 1, PushEventCallback(), &argument) == UserContextNotFound, "extended callback through a deleted user context");
    Require(sceNpWebApiDeleteHandle(context, handle) == 0, "delete the handle of the extended filters");
    Require(sceNpWebApiTerminate(context) == 0, "terminate the console-measured library context");
}

void TestOlderSdks() {
    const int context = sceNpWebApiInitialize(1, 0x10000);
    const int user = sceNpWebApiCreateContext(context, onlineId);
    Require(user > 0, "user context for older SDKs");
    int argument = 0;

    SetSdkVersion(0x02000000);
    std::int64_t request = 0;
    Require(sceNpWebApiCreateRequest(user, "group", "/path", MethodPatch, nullptr, &request) == 0, "PATCH before SDK 2.50");
    Require(sceNpWebApiSendRequest(request, nullptr, 0) == NotSignedIn, "send a PATCH request");
    Require(sceNpWebApiRegisterPushEventCallback(user, 1, nullptr, &argument) == InvalidArgument, "push event callback without a function from SDK 1.00");
    const int callback = sceNpWebApiRegisterPushEventCallback(user, 1, PushEventCallback(), &argument);
    Require(callback > 0, "push event callback without a filter before SDK 2.50");
    Require(sceNpWebApiUnregisterPushEventCallback(user, callback) == 0, "unregister the unfiltered push event callback");
    const int service = sceNpWebApiRegisterServicePushEventCallback(user, 1, PushEventCallback(), &argument);
    Require(service > 0 && service != callback, "service callback without a filter before SDK 2.50");
    Require(sceNpWebApiUnregisterPushEventCallback(user, service) == PushEventCallbackNotFound, "a service callback is not a push event callback");
    Require(sceNpWebApiUnregisterServicePushEventCallback(user, service) == 0, "unregister the service callback");
    Require(sceNpWebApiUnregisterServicePushEventCallback(user, service) == ServicePushEventCallbackNotFound, "service callback unregistered twice");

    SetSdkVersion(0x01000000);
    const int handle = sceNpWebApiCreateHandle(context);
    Require(sceNpWebApiCreateServicePushEventFilter(context, handle, "service", 0xffffffffu, filterParam, 1) == SignedInUserNotFound, "the invalid service label is accepted before SDK 2.00");

    SetSdkVersion(0x00900000);
    const int unchecked = sceNpWebApiRegisterPushEventCallback(user, 1, nullptr, &argument);
    Require(unchecked > 0, "push event callback without a function before SDK 1.00");
    Require(sceNpWebApiUnregisterPushEventCallback(user, unchecked) == 0, "unregister the push event callback without a function");
    Require(sceNpWebApiRegisterServicePushEventCallback(user + 1, 1, nullptr, &argument) == UserContextNotFound, "the user context is checked before the service callback function before SDK 1.00");
    Require(sceNpWebApiRegisterServicePushEventCallback(user, 1, nullptr, &argument) == InvalidArgument, "service callback without a function before SDK 1.00");
    Require(sceNpWebApiIntRegisterServicePushEventCallback(user + 1, 1, nullptr, &argument) == InvalidArgument, "the int service callback function is checked before the user context before SDK 1.00");
    Require(sceNpWebApiIntRegisterServicePushEventCallbackA(user + 1, 1, nullptr, &argument) == InvalidArgument, "the int service callback A function is checked before the user context before SDK 1.00");
    Require(sceNpWebApiIntRegisterServicePushEventCallback(user, 1, nullptr, &argument) == InvalidArgument, "int service callback without a function before SDK 1.00");
    Require(sceNpWebApiIntRegisterServicePushEventCallbackA(user, 1, nullptr, &argument) == InvalidArgument, "int service callback A without a function before SDK 1.00");
    const int intOld = sceNpWebApiIntRegisterServicePushEventCallback(user, 1, PushEventCallback(), &argument);
    Require(intOld > 0, "int service callback before SDK 1.00");
    Require(sceNpWebApiUnregisterServicePushEventCallback(user, intOld) == 0, "unregister the int service callback before SDK 1.00");
    const int intOldA = sceNpWebApiIntRegisterServicePushEventCallbackA(user, 1, PushEventCallback(), &argument);
    Require(intOldA > 0 && intOldA != intOld, "int service callback A before SDK 1.00");
    Require(sceNpWebApiUnregisterServicePushEventCallback(user, intOldA) == 0, "unregister the int service callback A before SDK 1.00");

    SetSdkVersion(0x04508001);
    Require(sceNpWebApiTerminate(context) == 0, "terminate the older SDK library context");
}

void TestHandleOperations() {
    const int context = sceNpWebApiInitialize(1, 0x10000);
    const int other = sceNpWebApiInitialize(1, 0x10000);
    const int handle = sceNpWebApiCreateHandle(context);
    Require(context > 0 && other > 0 && handle > 0, "library contexts and handle for handle operations");

    Require(sceNpWebApiAbortHandle(0x8000, handle) == LibContextNotFound, "abort a handle of an unknown context");
    Require(sceNpWebApiAbortHandle(other, handle) == HandleNotFound, "abort a handle through another context");
    Require(sceNpWebApiAbortHandle(context, handle) == 0, "abort the handle");
    Require(sceNpWebApiAbortHandle(context, handle) == 0, "abort the handle twice");
    Require(sceNpWebApiSetHandleTimeout(0x8000, handle, 1000) == LibContextNotFound, "handle timeout of an unknown context");
    Require(sceNpWebApiSetHandleTimeout(other, handle, 1000) == HandleNotFound, "handle timeout through another context");
    Require(sceNpWebApiSetHandleTimeout(context, handle, 1000) == 0, "set the handle timeout");
    Require(sceNpWebApiSetHandleTimeout(context, handle, 0) == 0, "clear the handle timeout");
    Require(sceNpWebApiDeleteHandle(context, handle) == 0, "delete the aborted handle");
    Require(sceNpWebApiAbortHandle(context, handle) == HandleNotFound, "abort a deleted handle");
    Require(sceNpWebApiSetHandleTimeout(context, handle, 1000) == HandleNotFound, "timeout of a deleted handle");

    SetSdkVersion(0x02500000);
    const int older = sceNpWebApiCreateHandle(context);
    Require(older > 0, "handle before SDK 3.00");
    Require(sceNpWebApiSetHandleTimeout(context, older, 1000) == HandleNotFound, "a handle created before SDK 3.00 has no timer");
    Require(sceNpWebApiAbortHandle(context, older) == 0, "abort a handle created before SDK 3.00");
    SetSdkVersion(0x04508001);
    Require(sceNpWebApiSetHandleTimeout(context, older, 1000) == HandleNotFound, "the timer is decided when the handle is created");
    Require(sceNpWebApiDeleteHandle(context, older) == 0, "delete the handle created before SDK 3.00");

    Require(sceNpWebApiTerminate(context) == 0 && sceNpWebApiTerminate(other) == 0, "terminate the handle library contexts");
}

void TestRequestOptions() {
    const int context = sceNpWebApiInitialize(1, 0x10000);
    const int user = sceNpWebApiCreateContext(context, onlineId);
    Require(user > 0, "user context for request options");
    std::int64_t request = 0;
    Require(sceNpWebApiCreateRequest(user, "group", "/path", MethodGet, nullptr, &request) == 0, "request for options");
    const std::int64_t unknownUser = static_cast<std::int64_t>(user + 1) << 32 | 1;

    Require(sceNpWebApiAddHttpRequestHeader(request, nullptr, "value") == InvalidArgument, "header without a field name");
    Require(sceNpWebApiAddHttpRequestHeader(request, "Accept", nullptr) == InvalidArgument, "header without a value");
    Require(sceNpWebApiAddHttpRequestHeader(-1, nullptr, "value") == InvalidArgument, "the header is checked before the request");
    Require(sceNpWebApiAddHttpRequestHeader(-1, "Accept", "application/json") == LibContextNotFound, "header of request -1");
    Require(sceNpWebApiAddHttpRequestHeader(unknownUser, "Accept", "application/json") == UserContextNotFound, "header of a request of an unknown user context");
    Require(sceNpWebApiAddHttpRequestHeader(request + 0x10000, "Accept", "application/json") == RequestNotFound, "header of an unknown request");
    Require(sceNpWebApiAddHttpRequestHeader(request, "Accept", "application/json") == 0, "add a header");
    Require(sceNpWebApiAddHttpRequestHeader(request, "Accept", "application/json") == 0, "add the same header twice");

    Require(sceNpWebApiSetMultipartContentType(-1, "multipart/mixed", "boundary") == LibContextNotFound, "multipart type of request -1");
    Require(sceNpWebApiSetMultipartContentType(unknownUser, "multipart/mixed", "boundary") == UserContextNotFound, "multipart type of a request of an unknown user context");
    Require(sceNpWebApiSetMultipartContentType(request + 0x10000, "multipart/mixed", "boundary") == RequestNotFound, "multipart type of an unknown request");
    Require(sceNpWebApiSetMultipartContentType(request, nullptr, nullptr) == 0, "multipart type without a name or boundary");
    Require(sceNpWebApiSetMultipartContentType(request, "multipart/mixed", "boundary") == 0, "multipart type of a request that is not multipart");

    Require(sceNpWebApiSetRequestTimeout(-1, 1000) == LibContextNotFound, "timeout of request -1");
    Require(sceNpWebApiSetRequestTimeout(unknownUser, 1000) == UserContextNotFound, "timeout of a request of an unknown user context");
    Require(sceNpWebApiSetRequestTimeout(request + 0x10000, 1000) == RequestNotFound, "timeout of an unknown request");
    Require(sceNpWebApiSetRequestTimeout(request, 0) == 0, "request without a timeout");

    std::array<char, 32> information{};
    information.fill('x');
    Require(sceNpWebApiSendRequest2(request, body, sizeof(body), information.data()) == NotSignedIn, "a request cannot be sent without a signed-in user");
    Require(sceNpWebApiSendRequest2(request, nullptr, 0, nullptr) == NotSignedIn, "send without response information");
    Require(information[0] == 'x' && information[31] == 'x', "a failed send leaves the response information alone");
    Require(sceNpWebApiSendRequest2(-1, nullptr, 0, nullptr) == LibContextNotFound, "send request -1");
    Require(sceNpWebApiSendRequest2(unknownUser, nullptr, 0, nullptr) == UserContextNotFound, "send a request of an unknown user context");
    Require(sceNpWebApiSendRequest2(request + 0x10000, nullptr, 0, nullptr) == RequestNotFound, "send an unknown request");
    Require(sceNpWebApiAbortRequest(request) == 0, "abort the request with options");
    Require(sceNpWebApiSendRequest2(request, nullptr, 0, nullptr) == Aborted, "send an aborted request");

    Require(sceNpWebApiDeleteRequest(request) == 0, "delete the request with options");
    Require(sceNpWebApiAddHttpRequestHeader(request, "Accept", "application/json") == RequestNotFound, "header of a deleted request");
    Require(sceNpWebApiTerminate(context) == 0, "terminate the request options library context");
}

void TestMultipartRequests() {
    const int context = sceNpWebApiInitialize(1, 0x10000);
    const int user = sceNpWebApiCreateContext(context, onlineId);
    Require(user > 0, "user context for multipart requests");

    std::int64_t request = 0;
    Require(sceNpWebApiCreateMultipartRequest(user, nullptr, "/path", MethodPost, &request) == InvalidArgument, "multipart request without an API group");
    Require(sceNpWebApiCreateMultipartRequest(user, "group", nullptr, MethodPost, &request) == InvalidArgument, "multipart request without a path");
    Require(sceNpWebApiCreateMultipartRequest(0, "group", "/path", MethodPatch, &request) == InvalidArgument, "the method is checked before the user context from SDK 2.50");
    Require(sceNpWebApiCreateMultipartRequest(user + 1, "group", "/path", MethodPost, &request) == UserContextNotFound, "multipart request of an unknown user context");
    Require(sceNpWebApiCreateMultipartRequest(0, "group", "/path", MethodPost, &request) == LibContextNotFound, "multipart request of user context 0");
    Require(request == 0, "a failed multipart request leaves the id alone");
    Require(sceNpWebApiCreateMultipartRequest(user, "group", "/path", MethodPost, nullptr) == 0, "multipart request without an id output");
    Require(sceNpWebApiCreateMultipartRequest(user, "group", "/path", MethodPost, &request) == 0, "create a multipart request");
    Require(static_cast<int>(request >> 32) == user && (request & 0xffffffff) != 0, "multipart request id holds its user context");

    Require(sceNpWebApiSetMultipartContentType(request, "multipart/mixed", "boundary") == 0, "multipart type");
    Require(sceNpWebApiAddHttpRequestHeader(request, "Accept", "application/json") == 0, "header of a multipart request");
    Require(sceNpWebApiSendRequest(request, body, sizeof(body)) == InvalidArgument, "a multipart request has no part 0");
    Require(sceNpWebApiSendRequest2(request, body, sizeof(body), nullptr) == InvalidArgument, "a multipart request has no part 0 for SendRequest2");
    Require(sceNpWebApiAbortRequest(request) == 0, "abort the multipart request");
    Require(sceNpWebApiSendRequest(request, nullptr, 0) == InvalidArgument, "the part is checked before the abort");
    Require(sceNpWebApiDeleteRequest(request) == 0, "delete the multipart request");
    Require(sceNpWebApiSendRequest(request, nullptr, 0) == RequestNotFound, "send a deleted multipart request");

    SetSdkVersion(0x02000000);
    Require(sceNpWebApiCreateMultipartRequest(user, "group", "/path", MethodPatch, &request) == 0, "multipart PATCH before SDK 2.50");
    SetSdkVersion(0x04508001);
    Require(sceNpWebApiTerminate(context) == 0, "terminate the multipart library context");
}

void TestRequestTimeouts() {
    sceNpWebApiCheckTimeout();
    const int context = sceNpWebApiInitialize(1, 0x10000);
    const int user = sceNpWebApiCreateContext(context, onlineId);
    Require(user > 0, "user context for timeouts");
    std::int64_t timed = 0;
    std::int64_t untimed = 0;
    std::int64_t unsent = 0;
    std::int64_t distant = 0;
    Require(sceNpWebApiCreateRequest(user, "group", "/path", MethodGet, nullptr, &timed) == 0, "request with a timeout");
    Require(sceNpWebApiCreateRequest(user, "group", "/path", MethodGet, nullptr, &untimed) == 0, "request without a timeout");
    Require(sceNpWebApiCreateRequest(user, "group", "/path", MethodGet, nullptr, &unsent) == 0, "request with a timeout that is never sent");
    Require(sceNpWebApiCreateRequest(user, "group", "/path", MethodGet, nullptr, &distant) == 0, "request with a distant timeout");
    Require(sceNpWebApiSetRequestTimeout(timed, 1) == 0 && sceNpWebApiSetRequestTimeout(unsent, 1) == 0, "set short timeouts");
    Require(sceNpWebApiSetRequestTimeout(distant, 0xffffffffu) == 0, "set a distant timeout");

    Require(sceNpWebApiSendRequest(timed, nullptr, 0) == NotSignedIn, "sending starts the request timer");
    Require(sceNpWebApiSendRequest(untimed, nullptr, 0) == NotSignedIn, "send the request without a timeout");
    Require(sceNpWebApiSendRequest(distant, nullptr, 0) == NotSignedIn, "send the request with a distant timeout");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    sceNpWebApiCheckTimeout();

    Require(sceNpWebApiAbortRequest(timed) == 0, "abort a timed out request");
    Require(sceNpWebApiSendRequest(timed, nullptr, 0) == NotSignedIn, "a timed out request is not reported as aborted");
    Require(sceNpWebApiAbortRequest(untimed) == 0 && sceNpWebApiSendRequest(untimed, nullptr, 0) == Aborted, "a request without a timeout does not time out");
    Require(sceNpWebApiAbortRequest(unsent) == 0 && sceNpWebApiSendRequest(unsent, nullptr, 0) == Aborted, "the timer starts when the request is sent");
    Require(sceNpWebApiAbortRequest(distant) == 0 && sceNpWebApiSendRequest(distant, nullptr, 0) == Aborted, "a distant timeout has not passed");

    Require(sceNpWebApiTerminate(context) == 0, "terminate the timeout library context");
    sceNpWebApiCheckTimeout();
}

void TestMultipartParts() {
    const int context = sceNpWebApiInitialize(1, 0x10000);
    const int user = sceNpWebApiCreateContext(context, onlineId);
    Require(user > 0, "user context for multipart parts");

    std::int64_t multipartRequest = 0;
    Require(sceNpWebApiCreateMultipartRequest(user, "group", "/path", MethodPost, &multipartRequest) == 0, "create multipart request");

    const std::int64_t unknownUser = static_cast<std::int64_t>(user + 1) << 32 | 1;

    int index = 0;
    Require(sceNpWebApiAddMultipartPart(multipartRequest, nullptr, &index) == InvalidArgument, "null multipart parameter");
    Require(sceNpWebApiAddMultipartPart(-1, nullptr, &index) == InvalidArgument, "null multipart parameter before lookup");
    Require(index == 0, "failed add part leaves index unmodified");

    const NpWebApiHttpHeader headers[2] = {{"Header1", "Val1"}, {"Content-Length", "100"}};
    const NpWebApiMultipartPartParameter validParam{headers, 2, 10};

    Require(sceNpWebApiAddMultipartPart(-1, &validParam, &index) == LibContextNotFound, "add part to request -1");
    Require(sceNpWebApiAddMultipartPart(unknownUser, &validParam, &index) == UserContextNotFound, "add part to unknown user context");
    Require(sceNpWebApiAddMultipartPart(multipartRequest + 0x10000, &validParam, &index) == RequestNotFound, "add part to unknown request");
    const NpWebApiMultipartPartParameter nullHeaders{nullptr, 1, 10};
    Require(sceNpWebApiAddMultipartPart(-1, &nullHeaders, &index) == LibContextNotFound, "null headers before library lookup");
    Require(sceNpWebApiAddMultipartPart(unknownUser, &nullHeaders, &index) == UserContextNotFound, "null headers before user lookup");
    Require(sceNpWebApiAddMultipartPart(multipartRequest + 0x10000, &nullHeaders, &index) == RequestNotFound, "null headers before request lookup");

    Require(sceNpWebApiAddMultipartPart(multipartRequest, &validParam, &index) == 0, "add first multipart part");
    Require(index == 1, "first part index is 1");

    int index2 = 0;
    const NpWebApiMultipartPartParameter paramNoHeaders{nullptr, 0, 20};
    Require(sceNpWebApiAddMultipartPart(multipartRequest, &paramNoHeaders, &index2) == 0, "add second multipart part");
    Require(index2 == 2, "second part index is 2");

    const NpWebApiMultipartPartParameter paramNoIndex{nullptr, 0, 5};
    Require(sceNpWebApiAddMultipartPart(multipartRequest, &paramNoIndex, nullptr) == 0, "add part with null index pointer");

    Require(sceNpWebApiSendMultipartRequest(multipartRequest, 0, body, sizeof(body)) == InvalidArgument, "partIndex 0 is invalid");
    Require(sceNpWebApiSendMultipartRequest(multipartRequest, -1, body, sizeof(body)) == InvalidArgument, "partIndex -1 is invalid");
    Require(sceNpWebApiSendMultipartRequest(-1, 0, body, sizeof(body)) == InvalidArgument, "partIndex 0 checked before lookup");
    Require(sceNpWebApiSendMultipartRequest(multipartRequest, 1, nullptr, sizeof(body)) == InvalidArgument, "null data is invalid");
    Require(sceNpWebApiSendMultipartRequest(-1, 1, nullptr, sizeof(body)) == InvalidArgument, "null data checked before lookup");
    Require(sceNpWebApiSendMultipartRequest(multipartRequest, 1, body, 0) == InvalidArgument, "dataSize 0 is invalid");
    Require(sceNpWebApiSendMultipartRequest(-1, 1, body, 0) == InvalidArgument, "dataSize 0 checked before lookup");

    Require(sceNpWebApiSendMultipartRequest2(multipartRequest, 0, body, sizeof(body), nullptr) == InvalidArgument, "send2 partIndex 0");
    Require(sceNpWebApiSendMultipartRequest2(multipartRequest, 1, nullptr, sizeof(body), nullptr) == InvalidArgument, "send2 null data");
    Require(sceNpWebApiSendMultipartRequest2(multipartRequest, 1, body, 0, nullptr) == InvalidArgument, "send2 dataSize 0");

    Require(sceNpWebApiSendMultipartRequest(-1, 1, body, sizeof(body)) == LibContextNotFound, "send multipart request -1");
    Require(sceNpWebApiSendMultipartRequest(unknownUser, 1, body, sizeof(body)) == UserContextNotFound, "send multipart unknown user");
    Require(sceNpWebApiSendMultipartRequest(multipartRequest + 0x10000, 1, body, sizeof(body)) == RequestNotFound, "send multipart unknown request");
    Require(sceNpWebApiSendMultipartRequest2(-1, 1, body, sizeof(body), nullptr) == LibContextNotFound, "send2 multipart request -1");

    std::int64_t emptyMultipart = 0;
    Require(sceNpWebApiCreateMultipartRequest(user, "group", "/path", MethodPost, &emptyMultipart) == 0, "create empty multipart request");
    Require(sceNpWebApiSendMultipartRequest(emptyMultipart, 1, body, sizeof(body)) == InvalidArgument, "partIndex out of range on empty multipart request");

    Require(sceNpWebApiSendMultipartRequest(multipartRequest, 4, body, sizeof(body)) == InvalidArgument, "partIndex out of range on multipart request");

    std::int64_t chunkReq = 0;
    Require(sceNpWebApiCreateMultipartRequest(user, "group", "/path", MethodPost, &chunkReq) == 0, "create chunked multipart request");
    int cIndex1 = 0;
    int cIndex2 = 0;
    const NpWebApiMultipartPartParameter chunkParam1{nullptr, 0, 10};
    const NpWebApiMultipartPartParameter chunkParam2{nullptr, 0, 20};
    Require(sceNpWebApiAddMultipartPart(chunkReq, &chunkParam1, &cIndex1) == 0, "add chunk part 1");
    Require(sceNpWebApiAddMultipartPart(chunkReq, &chunkParam2, &cIndex2) == 0, "add chunk part 2");
    Require(cIndex1 == 1 && cIndex2 == 2, "chunk part indices");

    Require(sceNpWebApiSendMultipartRequest(chunkReq, 1, "1234", 4) == 0, "partial send part 1");
    Require(sceNpWebApiSendMultipartRequest(chunkReq, 1, "567890", 6) == 0, "complete part 1");

    std::array<char, 32> respInfo{};
    respInfo.fill('z');
    Require(sceNpWebApiSendMultipartRequest2(chunkReq, 2, "0123456789", 10, respInfo.data()) == 0, "partial send part 2 with send2");
    Require(respInfo[0] == 'z' && respInfo[31] == 'z', "response information untouched on partial send");

    Require(sceNpWebApiSendMultipartRequest2(chunkReq, 2, "0123456789", 10, respInfo.data()) == NotSignedIn, "final send completes and returns NotSignedIn");
    Require(respInfo[0] == 'z' && respInfo[31] == 'z', "response information untouched on completed send");

    std::int64_t abortReq = 0;
    Require(sceNpWebApiCreateMultipartRequest(user, "group", "/path", MethodPost, &abortReq) == 0, "create abort multipart request");
    int aIndex = 0;
    const NpWebApiMultipartPartParameter abortParam{nullptr, 0, 10};
    Require(sceNpWebApiAddMultipartPart(abortReq, &abortParam, &aIndex) == 0, "add abort part");
    Require(sceNpWebApiSendMultipartRequest(abortReq, 1, "12345", 5) == 0, "partial send before abort");
    Require(sceNpWebApiAbortRequest(abortReq) == 0, "abort multipart request");
    Require(sceNpWebApiSendMultipartRequest(abortReq, 1, "67890", 5) == Aborted, "completed aborted multipart request returns Aborted");

    std::int64_t timedReq = 0;
    Require(sceNpWebApiCreateMultipartRequest(user, "group", "/path", MethodPost, &timedReq) == 0, "create timed multipart request");
    int tIndex = 0;
    const NpWebApiMultipartPartParameter timedParam{nullptr, 0, 10};
    Require(sceNpWebApiAddMultipartPart(timedReq, &timedParam, &tIndex) == 0, "add timed part");
    Require(sceNpWebApiSetRequestTimeout(timedReq, 1) == 0, "set timeout on multipart request");
    Require(sceNpWebApiSendMultipartRequest(timedReq, 1, "12345", 5) == 0, "partial send arms timeout timer");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    sceNpWebApiCheckTimeout();
    Require(sceNpWebApiSendMultipartRequest(timedReq, 1, "67890", 5) == NotSignedIn, "timed out multipart request returns NotSignedIn");

    Require(sceNpWebApiDeleteRequest(multipartRequest) == 0, "delete multipart request");
    Require(sceNpWebApiDeleteRequest(emptyMultipart) == 0, "delete empty multipart request");
    Require(sceNpWebApiDeleteRequest(chunkReq) == 0, "delete chunked multipart request");
    Require(sceNpWebApiDeleteRequest(abortReq) == 0, "delete abort multipart request");
    Require(sceNpWebApiDeleteRequest(timedReq) == 0, "delete timed multipart request");
    Require(sceNpWebApiTerminate(context) == 0, "terminate multipart parts library context");
}

void TestUtilityParseNpId() {
    NpId npId{};
    Require(sceNpWebApiUtilityParseNpId(nullptr, &npId) == NpInvalidArgument, "null jsonNpId");
    Require(sceNpWebApiUtilityParseNpId(nullptr, nullptr) == NpInvalidArgument, "null jsonNpId and null npId");
    Require(sceNpWebApiUtilityParseNpId("cGxheWVy", nullptr) == 0, "null npId with valid jsonNpId");

    std::memset(&npId, 0xCC, sizeof(npId));
    Require(sceNpWebApiUtilityParseNpId("cGxheWVyQHNlZzEvc2VnMi5zZWcz", &npId) == 0, "parse serialized npId");
    Require(std::strcmp(npId.handle.data, "player") == 0, "handle matches string before at");
    Require(std::memcmp(npId.opt, "seg1seg2", 8) == 0, "opt concatenates segments");
    Require(npId.reserved[0] == 0x01, "reserved 0 is 1");
    for (std::size_t i = 1; i < sizeof(npId.reserved); ++i) {
        Require(npId.reserved[i] == 0, "reserved bytes zeroed");
    }

    std::memset(&npId, 0xCC, sizeof(npId));
    Require(sceNpWebApiUtilityParseNpId("dXNlcjEyMw==", &npId) == 0, "parse serialized npId without at");
    Require(std::strcmp(npId.handle.data, "user123") == 0, "handle without at");
    for (std::size_t i = 0; i < sizeof(npId.opt); ++i) {
        Require(npId.opt[i] == 0, "opt zeroed without at");
    }
    Require(npId.reserved[0] == 0x01, "reserved 0 is 1 without at");

    std::memset(&npId, 0xCC, sizeof(npId));
    Require(sceNpWebApiUtilityParseNpId("MDEyMzQ1Njc4OWFiY2RlZkVYVFJBQDEyLzM0", &npId) == 0, "parse long handle");
    Require(std::memcmp(npId.handle.data, "0123456789abcdef", 16) == 0, "handle truncated to 16");
    Require(npId.handle.data[16] == '\0', "handle null terminated");
    Require(std::memcmp(npId.opt, "1234\0\0\0\0", 8) == 0, "opt with separators removed");
    Require(npId.reserved[0] == 0x01, "reserved 0 is 1 for long handle");

    std::memset(&npId, 0xCC, sizeof(npId));
    Require(sceNpWebApiUtilityParseNpId("dXNlckBhL2IuYy9kLmU=", &npId) == 0, "filter slashes and dots");
    Require(std::strcmp(npId.handle.data, "user") == 0, "handle is user");
    Require(std::memcmp(npId.opt, "abcde\0\0\0", 8) == 0, "opt filters separators");
    Require(npId.reserved[0] == 0x01, "reserved 0 is 1");
}

void TestIntCreateCtxIndExtdPushEventFilter() {
    const int context = sceNpWebApiInitialize(1, 0x10000);
    const int other = sceNpWebApiInitialize(1, 0x10000);
    const int handle = sceNpWebApiCreateHandle(context);
    Require(context > 0 && other > 0 && handle > 0, "library contexts and handle for extended push event filter");
    unsigned char filterParam[64] = {};

    Require(sceNpWebApiIntCreateCtxIndExtdPushEventFilter(context, handle, nullptr, 1) == InvalidArgument, "extended push event filter without its parameter");
    Require(sceNpWebApiIntCreateCtxIndExtdPushEventFilter(context, handle, filterParam, 0) == InvalidArgument, "extended push event filter with no parameters");
    Require(sceNpWebApiIntCreateCtxIndExtdPushEventFilter(0x7fff + 1, handle, nullptr, 1) == InvalidArgument, "the argument is checked before the library context");
    Require(sceNpWebApiIntCreateCtxIndExtdPushEventFilter(0x7fff + 1, handle, filterParam, 1) == LibContextNotFound, "extended push event filter of an unknown context");
    Require(sceNpWebApiIntCreateCtxIndExtdPushEventFilter(other, handle, filterParam, 1) == HandleNotFound, "extended push event filter with another context's handle");

    Require(sceNpWebApiTerminate(context) == 0 && sceNpWebApiTerminate(other) == 0, "terminate the extended push event filter library contexts");
}

void TestConnectionChecks() {
    const int context = sceNpWebApiInitialize(1, 0x10000);
    const int other = sceNpWebApiInitialize(1, 0x10000);
    Require(context > 0 && other > 0 && sceNpWebApiTerminate(other) == 0, "library contexts for the connection checks");
    const int user = sceNpWebApiCreateContext(context, onlineId);
    Require(user > 0, "user context for the connection checks");
    unsigned char stats[64] = {};

    Require(sceNpWebApiClearUnusedConnection(user, nullptr, false) == InvalidArgument, "clear connections without a target");
    Require(sceNpWebApiClearUnusedConnection(-1, nullptr, true) == InvalidArgument, "the clear target is checked before the contexts");
    Require(sceNpWebApiClearUnusedConnection((other << 16) | 1, "group", false) == LibContextNotFound, "clear connections of a terminated library context");
    Require(sceNpWebApiClearUnusedConnection(-1, "group", false) == LibContextNotFound, "clear connections of user context -1");
    Require(sceNpWebApiClearUnusedConnection(user + 1, "group", true) == UserContextNotFound, "clear connections of an unknown user context");

    Require(sceNpWebApiGetConnectionStats(user, nullptr, stats) == InvalidArgument, "connection stats without a target");
    Require(sceNpWebApiGetConnectionStats(user, "group", nullptr) == InvalidArgument, "connection stats without an output");
    Require(sceNpWebApiGetConnectionStats(-1, nullptr, nullptr) == InvalidArgument, "the stats arguments are checked before the contexts");
    Require(sceNpWebApiGetConnectionStats((other << 16) | 1, "group", stats) == LibContextNotFound, "connection stats of a terminated library context");
    Require(sceNpWebApiGetConnectionStats(user + 1, "group", stats) == UserContextNotFound, "connection stats of an unknown user context");
    for (const unsigned char byte : stats) Require(byte == 0, "a failed stats call writes nothing");

    Require(sceNpWebApiDeleteContext(user) == 0, "delete the user context of the connection checks");
    Require(sceNpWebApiClearUnusedConnection(user, "group", false) == UserContextNotFound, "clear connections of a deleted user context");
    Require(sceNpWebApiTerminate(context) == 0, "terminate the connection checks library context");
}

}

int main() {
    InstallProcessParameters();
    SetSdkVersion(0x04508001);
    TestUnknownFieldChecks();
    TestLibraryContexts();
    TestIntServicePushEventFilter();
    TestUserContexts();
    TestRequests();
    TestResponses();
    TestPushEventCallbacks();
    TestConsoleMeasuredExports();
    TestOlderSdks();
    TestHandleOperations();
    TestRequestOptions();
    TestMultipartRequests();
    TestRequestTimeouts();
    TestMultipartParts();
    TestUtilityParseNpId();
    TestIntCreateCtxIndExtdPushEventFilter();
    TestConnectionChecks();
    return 0;
}
