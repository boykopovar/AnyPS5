#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI sceNpSessionSignalingInitialize(void* param);
int APS5_VABI sceNpSessionSignalingCreateContext2(const void* param, std::uint32_t* contextId);
std::int32_t APS5_VABI sceNpSessionSignalingGetLocalNetInfo(std::int32_t contextId, void* info);
int APS5_VABI sceNpSessionSignalingRequestPrepare(std::uint32_t contextId, std::uint32_t* requestId);
int APS5_VABI sceNpSessionSignalingTerminate(void);
}

namespace {

constexpr int InvalidArgument = static_cast<int>(0x80553303u);
constexpr int Unavailable = static_cast<int>(0x80552D06u);

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "NpSessionSignaling: %s\n", message);
        std::abort();
    }
}

}

int main() {
    unsigned char initializeParam[32] = {};
    Require(sceNpSessionSignalingInitialize(initializeParam) == 0, "initialize failed");
    unsigned char contextParam[64] = {};
    std::uint32_t contextId = 0;
    Require(sceNpSessionSignalingCreateContext2(contextParam, &contextId) == 0, "context creation failed");

    std::uint32_t requestId = 0xa5a5a5a5u;
    Require(sceNpSessionSignalingRequestPrepare(contextId, nullptr) == InvalidArgument, "null request id");
    Require(sceNpSessionSignalingRequestPrepare(contextId, &requestId) == Unavailable, "prepare without network");
    Require(requestId == 0xa5a5a5a5u, "failed prepare wrote a request id");

    const auto netContextId = static_cast<std::int32_t>(contextId);
    unsigned char info[16];
    std::memset(info, 0xa5, sizeof(info));
    unsigned char untouched[sizeof(info)];
    std::memcpy(untouched, info, sizeof(info));
    Require(sceNpSessionSignalingGetLocalNetInfo(netContextId, nullptr) == InvalidArgument, "null info");
    Require(sceNpSessionSignalingGetLocalNetInfo(netContextId, info) == Unavailable, "local net info without network");
    Require(std::memcmp(info, untouched, sizeof(info)) == 0, "failed query modified the info");

    Require(sceNpSessionSignalingTerminate() == 0, "terminate failed");
}
