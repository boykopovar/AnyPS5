#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>

extern "C" {
int APS5_VABI sceNpCheckNpAvailability(int req_id, const char* user, void* result);
int APS5_VABI sceNpGetNpId(int user_id, NpId* np_id);
void APS5_VABI sceNpRegisterGamePresenceCallback(void* callback, void* userdata);
int APS5_VABI sceNpSetContentRestriction(const NpContentRestriction* restriction);
int APS5_VABI sceNpAuthCreateRequest(void);
int APS5_VABI sceNpAuthGetIdTokenV3(int req_id, const void* param, void* id_token);
int APS5_VABI sceNpAuthWaitAsync(int req_id, int* result);
}

namespace {

constexpr int SceNpErrorInvalidArgument = static_cast<int>(0x80550003);
constexpr int SceNpErrorSignedOut = static_cast<int>(0x80550006);

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    NpId np_id{};
    Require(sceNpGetNpId(0, nullptr) == SceNpErrorInvalidArgument);
    Require(sceNpGetNpId(0, &np_id) == SceNpErrorSignedOut);

    Require(sceNpCheckNpAvailability(1, "Player", nullptr) == SceNpErrorSignedOut);

    int callback = 0;
    sceNpRegisterGamePresenceCallback(&callback, &callback);

    NpContentRestriction restriction{};
    Require(sceNpSetContentRestriction(nullptr) == SceNpErrorInvalidArgument);
    Require(sceNpSetContentRestriction(&restriction) == 0);

    const int first = sceNpAuthCreateRequest();
    const int second = sceNpAuthCreateRequest();
    Require(first > 0 && second > first);

    char token = 0;
    Require(sceNpAuthGetIdTokenV3(first, nullptr, &token) == SceNpErrorInvalidArgument);
    Require(sceNpAuthGetIdTokenV3(first, &token, nullptr) == SceNpErrorInvalidArgument);
    Require(sceNpAuthGetIdTokenV3(first, &token, &token) == SceNpErrorSignedOut);

    int result = 0;
    Require(sceNpAuthWaitAsync(first, &result) == 0);
    Require(result == SceNpErrorSignedOut);
    return 0;
}
