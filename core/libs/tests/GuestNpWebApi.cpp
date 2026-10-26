#include "ProcessParametersFixture.hpp"
#include "prx/libSceNpWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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

    Require(sceNpWebApiTerminate(context) == 0 && sceNpWebApiTerminate(other) == 0, "terminate the callback library contexts");
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

    SetSdkVersion(0x04508001);
    Require(sceNpWebApiTerminate(context) == 0, "terminate the older SDK library context");
}

}

int main() {
    InstallProcessParameters();
    SetSdkVersion(0x04508001);
    TestLibraryContexts();
    TestUserContexts();
    TestRequests();
    TestPushEventCallbacks();
    TestOlderSdks();
    return 0;
}
