#include "prx/libc/include/general/VabiMacros.hpp"
#include "SceTypes.hpp"
#include <cstdlib>

extern "C" {
int APS5_VABI sceNetCtlInit(void);
void APS5_VABI sceNetCtlTerm(void);
int APS5_VABI sceNetCtlCheckCallback(void);
int APS5_VABI sceNetCtlGetInfo(int code, NetCtlInfo* info);
int APS5_VABI sceNetCtlGetNatInfo(NetCtlNatInfo* nat_info);
int APS5_VABI sceNetCtlGetResult(int event_type, int* error_code);
int APS5_VABI sceNetCtlGetState(int* state);
int APS5_VABI sceNetCtlGetStateV6(int* state);
int APS5_VABI sceNetCtlRegisterCallback(NetCtlCallback func, void* arg, int* cid);
int APS5_VABI sceNetCtlUnregisterCallback(int cid);
}

static void Require(bool value) { if (!value) std::abort(); }

static void NoopNetCtlCallback(int, void*) {}

int main() {
    constexpr int CALLBACK_MAX = static_cast<int>(0x80412103);
    constexpr int INVALID_ID = static_cast<int>(0x80412105);
    constexpr int INVALID_ADDR = static_cast<int>(0x80412107);
    constexpr int NOT_CONNECTED = static_cast<int>(0x80412108);

    Require(sceNetCtlInit() == 0);
    Require(sceNetCtlCheckCallback() == 0);

    NetCtlInfo info{};
    Require(sceNetCtlGetInfo(0, &info) == NOT_CONNECTED);
    Require(sceNetCtlGetInfo(0, nullptr) == NOT_CONNECTED);

    NetCtlNatInfo nat{};
    Require(sceNetCtlGetNatInfo(&nat) == NOT_CONNECTED);
    Require(sceNetCtlGetNatInfo(nullptr) == NOT_CONNECTED);

    Require(sceNetCtlGetResult(0, nullptr) == INVALID_ADDR);
    int result = -1;
    Require(sceNetCtlGetResult(7, &result) == 0 && result == 0);

    int state = -1;
    Require(sceNetCtlGetState(nullptr) == INVALID_ADDR);
    Require(sceNetCtlGetState(&state) == 0 && (state == 0 || state == 3));
    state = -1;
    Require(sceNetCtlGetStateV6(nullptr) == INVALID_ADDR);
    Require(sceNetCtlGetStateV6(&state) == 0 && (state == 0 || state == 3));

    int callback_id = -1;
    Require(sceNetCtlRegisterCallback(nullptr, nullptr, &callback_id) == INVALID_ADDR);
    Require(sceNetCtlRegisterCallback(NoopNetCtlCallback, nullptr, nullptr) == INVALID_ADDR);

    for (int expected = 0; expected < 8; ++expected) {
        callback_id = -1;
        Require(sceNetCtlRegisterCallback(NoopNetCtlCallback, nullptr, &callback_id) == 0);
        Require(callback_id == expected);
    }
    Require(sceNetCtlRegisterCallback(NoopNetCtlCallback, nullptr, &callback_id) == CALLBACK_MAX);

    Require(sceNetCtlUnregisterCallback(-1) == INVALID_ID);
    Require(sceNetCtlUnregisterCallback(8) == INVALID_ID);

    Require(sceNetCtlUnregisterCallback(0) == 0);
    callback_id = -1;
    Require(sceNetCtlRegisterCallback(NoopNetCtlCallback, nullptr, &callback_id) == 0);
    Require(callback_id == 0);

    for (int id = 0; id < 8; ++id) {
        Require(sceNetCtlUnregisterCallback(id) == 0);
    }
    Require(sceNetCtlUnregisterCallback(3) == 0);

    sceNetCtlTerm();
    return 0;
}
