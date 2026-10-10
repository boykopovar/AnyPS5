#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" {
int APS5_VABI sceNpAuthCreateRequest(void);
int APS5_VABI sceNpAuthCreateAsyncRequest(const void* param);
int APS5_VABI sceNpAuthGetIdTokenV3(int req_id, const void* param, void* id_token);
int APS5_VABI sceNpAuthWaitAsync(int req_id, int* result);
int APS5_VABI sceNpAuthDeleteRequest(int req_id);
}

namespace {

constexpr int InvalidArgument = static_cast<int>(0x80550003u);
constexpr int SignedOut = static_cast<int>(0x80550006u);

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "NpAuth: %s\n", message);
        std::abort();
    }
}

}

int main() {
    const int first = sceNpAuthCreateRequest();
    const int second = sceNpAuthCreateRequest();
    Require(first > 0 && second > 0 && first != second, "sceNpAuthCreateRequest must return distinct positive ids");
    Require(sceNpAuthCreateAsyncRequest(nullptr) > 0, "sceNpAuthCreateAsyncRequest must keep returning an id");

    unsigned char param[64] = {};
    unsigned char token[4096] = {};
    Require(sceNpAuthGetIdTokenV3(first, param, token) == SignedOut, "sceNpAuthGetIdTokenV3 must report the user as signed out");
    for (unsigned char byte : token) Require(byte == 0, "sceNpAuthGetIdTokenV3 must leave the token untouched");
    Require(sceNpAuthGetIdTokenV3(first, nullptr, token) == InvalidArgument, "sceNpAuthGetIdTokenV3 must reject a null parameter");
    Require(sceNpAuthGetIdTokenV3(first, param, nullptr) == InvalidArgument, "sceNpAuthGetIdTokenV3 must reject a null token");

    int result = 0;
    Require(sceNpAuthWaitAsync(first, &result) == 0 && result == SignedOut, "sceNpAuthWaitAsync must finish with the signed-out result");
    Require(sceNpAuthWaitAsync(first, nullptr) == 0, "sceNpAuthWaitAsync must accept a null result like sceNpAuthPollAsync");

    Require(sceNpAuthDeleteRequest(first) == 0 && sceNpAuthDeleteRequest(second) == 0, "sceNpAuthDeleteRequest must succeed");
    return 0;
}
