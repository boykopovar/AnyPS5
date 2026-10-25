#include "prx/libSceNpWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int InvalidArgument = static_cast<int>(0x80552902u);
constexpr int LibContextNotFound = static_cast<int>(0x80552904u);
constexpr int PushEventFilterNotFound = static_cast<int>(0x8055290Bu);
constexpr int HandleNotFound = static_cast<int>(0x8055290Du);
constexpr int ServicePushEventFilterNotFound = static_cast<int>(0x8055290Eu);
constexpr int SignedInUserNotFound = static_cast<int>(0x80552910u);

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "NpWebApi: %s\n", message);
        std::abort();
    }
}

}

int main() {
    const int context = sceNpWebApiInitialize(1, 0x10000);
    Require(context > 0 && context <= 0x7fff, "library context id out of range");
    const int other = sceNpWebApiInitialize(1, 0x10000);
    Require(other > 0 && other != context, "a second library context must get its own id");
    Require(sceNpWebApiCreateHandle(0x7fff + 1) == LibContextNotFound, "handle of an unknown context");

    const int handle = sceNpWebApiCreateHandle(context);
    Require(handle > 0 && static_cast<unsigned>(handle) < 0xf0000000u, "handle id out of range");
    const char filterParam[16] = {};
    Require(sceNpWebApiCreatePushEventFilter(context, nullptr, 1) == InvalidArgument, "push event filter without its parameter");
    Require(sceNpWebApiCreatePushEventFilter(context, filterParam, 0) == InvalidArgument, "push event filter with no parameters");
    const int filter = sceNpWebApiCreatePushEventFilter(context, filterParam, 1);
    Require(filter > 0 && filter != handle, "push event filter id");

    Require(sceNpWebApiCreateServicePushEventFilter(context, handle, nullptr, 0, filterParam, 1) == InvalidArgument, "service filter without a service name");
    Require(sceNpWebApiCreateServicePushEventFilter(0x7fff + 1, handle, "service", 0, filterParam, 1) == LibContextNotFound, "service filter of an unknown context");
    Require(sceNpWebApiCreateServicePushEventFilter(other, handle, "service", 0, filterParam, 1) == HandleNotFound, "service filter with another context's handle");
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
    return 0;
}
