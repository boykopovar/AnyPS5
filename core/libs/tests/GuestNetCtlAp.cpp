#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>

extern "C" {
int APS5_VABI sceNetCtlApInit(void);
void APS5_VABI sceNetCtlApTerm(void);
int APS5_VABI sceNetCtlApCheckCallback(void);
int APS5_VABI sceNetCtlApClearEvent(int eventType);
int APS5_VABI sceNetCtlApRegisterCallback(void (APS5_VABI *func)(int, void*), void* arg, int* cid);
int APS5_VABI sceNetCtlApUnregisterCallback(int cid);
int APS5_VABI sceNetCtlApGetResult(int eventType, int* errorCode);
int APS5_VABI sceNetCtlApGetInfo(void* info);
int APS5_VABI sceNetCtlApGetState(int* state);
int APS5_VABI sceNetCtlApStop(void);
int APS5_VABI sceNetCtlApGetConnectInfo(const char* optStr, void* connectInfo);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kErrNotStarted = static_cast<int>(0x8041298F);
constexpr int kErrInvalidType = static_cast<int>(0x80412990);
constexpr int kErrInvalidAddr = static_cast<int>(0x80412991);
constexpr int kErrNotInitialized = static_cast<int>(0x80412994);
constexpr int kErrCallbackMax = static_cast<int>(0x80412995);
constexpr int kErrInvalidId = static_cast<int>(0x80412996);
constexpr int kErrIdNotFound = static_cast<int>(0x80412997);
constexpr int kErrInvalidSize = static_cast<int>(0x80412998);

int g_calls = 0;

void APS5_VABI Callback(int, void*) { ++g_calls; }

void RequireUninitialized() {
    int value = 0x5a5a;
    std::uint8_t block[184] = {};
    Require(sceNetCtlApCheckCallback() == kErrNotInitialized);
    Require(sceNetCtlApClearEvent(1) == kErrNotInitialized);
    Require(sceNetCtlApRegisterCallback(Callback, nullptr, &value) == kErrNotInitialized && value == 0x5a5a);
    Require(sceNetCtlApUnregisterCallback(0) == kErrNotInitialized);
    Require(sceNetCtlApGetResult(1, &value) == kErrNotInitialized && value == 0x5a5a);
    Require(sceNetCtlApGetInfo(block) == kErrNotInitialized);
    Require(sceNetCtlApGetState(&value) == kErrNotInitialized && value == 0x5a5a);
    Require(sceNetCtlApStop() == kErrNotInitialized);
    Require(sceNetCtlApGetConnectInfo(nullptr, block) == kErrNotInitialized);
}

}

int main() {
    RequireUninitialized();
    Require(sceNetCtlApInit() == 0);

    int state = -1;
    Require(sceNetCtlApGetState(&state) == 0 && state == 0);
    Require(sceNetCtlApGetState(nullptr) == kErrInvalidAddr);

    std::uint8_t info[152];
    std::memset(info, 0x5a, sizeof(info));
    const std::uint64_t infoSize = sizeof(info);
    std::memcpy(info, &infoSize, sizeof(infoSize));
    Require(sceNetCtlApGetInfo(info) == kErrNotStarted);
    for (std::size_t i = sizeof(infoSize); i < sizeof(info); ++i) Require(info[i] == 0x5a);
    Require(sceNetCtlApGetInfo(nullptr) == kErrInvalidAddr);
    const std::uint64_t wrongInfoSize = sizeof(info) - 8;
    std::memcpy(info, &wrongInfoSize, sizeof(wrongInfoSize));
    Require(sceNetCtlApGetInfo(info) == kErrInvalidSize);

    std::uint8_t connect[184];
    std::memset(connect, 0x5a, sizeof(connect));
    const std::uint64_t connectSize = sizeof(connect);
    std::memcpy(connect, &connectSize, sizeof(connectSize));
    const char option[] = "opt";
    Require(sceNetCtlApGetConnectInfo(option, connect) == kErrNotStarted);
    Require(sceNetCtlApGetConnectInfo(nullptr, connect) == kErrNotStarted);
    Require(sceNetCtlApGetConnectInfo("", connect) == kErrNotStarted);
    Require(sceNetCtlApGetConnectInfo("12345678", connect) == kErrNotStarted);
    Require(sceNetCtlApGetConnectInfo("123456789", nullptr) == static_cast<int>(0x80412901u));
    Require(sceNetCtlApGetConnectInfo("123456789", connect) == static_cast<int>(0x80412901u));
    for (std::size_t i = sizeof(connectSize); i < sizeof(connect); ++i) Require(connect[i] == 0x5a);
    Require(sceNetCtlApGetConnectInfo(option, nullptr) == kErrInvalidAddr);
    const std::uint64_t wrongConnectSize = sizeof(info);
    std::memcpy(connect, &wrongConnectSize, sizeof(wrongConnectSize));
    Require(sceNetCtlApGetConnectInfo(option, connect) == kErrInvalidSize);

    Require(sceNetCtlApStop() == kErrNotStarted);

    for (int type = 1; type <= 4; ++type) {
        int code = -1;
        Require(sceNetCtlApClearEvent(type) == 0);
        Require(sceNetCtlApGetResult(type, &code) == 0 && code == 0);
        Require(sceNetCtlApGetResult(type, nullptr) == kErrInvalidAddr);
    }
    for (const int type : {0, 5, -1}) {
        int code = 0x5a5a;
        Require(sceNetCtlApClearEvent(type) == kErrInvalidType);
        Require(sceNetCtlApGetResult(type, &code) == kErrInvalidType && code == 0x5a5a);
        Require(sceNetCtlApGetResult(type, nullptr) == kErrInvalidAddr);
    }

    int cids[8];
    for (int i = 0; i < 8; ++i) {
        cids[i] = -1;
        Require(sceNetCtlApRegisterCallback(Callback, &cids[i], &cids[i]) == 0 && cids[i] == i);
    }
    int extra = 0x5a5a;
    Require(sceNetCtlApRegisterCallback(Callback, nullptr, &extra) == kErrCallbackMax && extra == -1);
    Require(sceNetCtlApRegisterCallback(nullptr, nullptr, &extra) == kErrInvalidAddr);
    Require(sceNetCtlApRegisterCallback(Callback, nullptr, nullptr) == kErrInvalidAddr);
    Require(sceNetCtlApCheckCallback() == 0 && sceNetCtlApCheckCallback() == 0);
    Require(g_calls == 0);
    Require(sceNetCtlApUnregisterCallback(3) == 0);
    Require(sceNetCtlApUnregisterCallback(3) == kErrIdNotFound);
    Require(sceNetCtlApUnregisterCallback(-1) == kErrInvalidId);
    Require(sceNetCtlApUnregisterCallback(8) == kErrInvalidId);
    Require(sceNetCtlApRegisterCallback(Callback, nullptr, &extra) == 0 && extra == 3);

    bool threw = false;
    try {
        sceNetCtlApInit();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    sceNetCtlApTerm();
    RequireUninitialized();
    Require(sceNetCtlApInit() == 0);
    int cid = -1;
    Require(sceNetCtlApRegisterCallback(Callback, nullptr, &cid) == 0 && cid == 0);
    Require(sceNetCtlApGetState(&state) == 0 && state == 0);
    sceNetCtlApTerm();
    sceNetCtlApTerm();
    RequireUninitialized();
}
