#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" {
int APS5_VABI sceNpWebApi2CreateRequest(int userContextId, const char* apiGroup, const char* path, const char* method, const void* contentParameter, std::int64_t* requestId);
int APS5_VABI sceNpWebApi2DeleteRequest(std::int64_t requestId);
int APS5_VABI sceNpWebApi2SetRequestTimeout(std::int64_t requestId, std::uint32_t timeout);
}

namespace {

constexpr int requestNotFound = static_cast<int>(0x80553406);

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "NpWebApi2: %s\n", message);
        std::abort();
    }
}

}

int main() {
    std::int64_t first = 0;
    std::int64_t second = 0;
    Require(sceNpWebApi2CreateRequest(1, "sce", "/v1/test", "GET", nullptr, &first) == 0, "first request creation failed");
    Require(sceNpWebApi2CreateRequest(1, "sce", "/v1/test", "GET", nullptr, &second) == 0, "second request creation failed");
    Require(first != second, "request ids repeat");
    Require(sceNpWebApi2SetRequestTimeout(first, 10 * 1000 * 1000) == 0, "timeout on a live request was rejected");
    Require(sceNpWebApi2SetRequestTimeout(first, 0) == 0, "disabling the timeout was rejected");
    Require(sceNpWebApi2SetRequestTimeout(second, UINT32_MAX) == 0, "the largest timeout was rejected");
    Require(sceNpWebApi2SetRequestTimeout(second + 1000, 1000) == requestNotFound, "an unknown request id was accepted");
    Require(sceNpWebApi2DeleteRequest(first) == 0, "request deletion failed");
    Require(sceNpWebApi2SetRequestTimeout(first, 1000) == requestNotFound, "a deleted request id was accepted");
    Require(sceNpWebApi2SetRequestTimeout(second, 1000) == 0, "deleting one request affected another");
    Require(sceNpWebApi2DeleteRequest(second) == 0, "second request deletion failed");
    std::puts("NpWebApi2 tests passed");
    return 0;
}
