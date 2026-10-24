#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceCompanionHttpdOptParamInitialize(void* param);
int APS5_VABI sceCompanionHttpdInitialize(const void* param);
int APS5_VABI sceCompanionHttpdTerminate(void);
int APS5_VABI sceCompanionHttpdRegisterRequestCallback(void* function, void* argument);
int APS5_VABI sceCompanionHttpdUnregisterRequestCallback(void);
int APS5_VABI sceCompanionHttpdStart(void);
int APS5_VABI sceCompanionHttpdStop(void);
int APS5_VABI sceCompanionHttpdGetEvent(void* event);
int APS5_VABI sceCompanionHttpdAddHeader(const char* key, const char* value, void* response);
int APS5_VABI sceCompanionHttpdSetStatus(int status, void* response);
int APS5_VABI sceCompanionHttpdSetBody(const char* body, std::uint64_t size, void* response);
int APS5_VABI sceCompanionHttpdGet2ndScreenStatus(void);
int APS5_VABI sceCompanionHttpdGetUserId(void);
int APS5_VABI sceCompanionHttpdInitialize2(void);
int APS5_VABI sceCompanionHttpdRegisterRequestBodyReceptionCallback(void);
int APS5_VABI sceCompanionHttpdRegisterRequestCallback2(void);
int APS5_VABI sceCompanionHttpdUnregisterRequestBodyReceptionCallback(void);
}

namespace {

constexpr int InvalidParam = static_cast<int>(0x80E40004u);
constexpr int NotInitialized = static_cast<int>(0x80E40006u);
constexpr int AlreadyInitialized = static_cast<int>(0x80E40007u);
constexpr int NoEvent = static_cast<int>(0x80E40008u);
constexpr int AlreadyStarted = static_cast<int>(0x80E4000Au);
constexpr int NotStarted = static_cast<int>(0x80E4000Bu);
constexpr int AlreadyRegistered = static_cast<int>(0x80E4000Cu);

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "CompanionHttpd: %s\n", message);
        std::abort();
    }
}

template <typename TCall>
void RequireNotImplemented(TCall call, const std::string& name) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()) == name + " not implemented", "a companion function threw another error");
        return;
    }
    Require(false, "a companion function that should throw returned");
}

int APS5_VABI RequestCallback(int, int, const void*, void*, void*) {
    std::fprintf(stderr, "CompanionHttpd: the request callback was called\n");
    std::abort();
}

void CheckOptionBlock() {
    unsigned char block[0x70];
    std::memset(block, 0x5a, sizeof(block));
    Require(sceCompanionHttpdOptParamInitialize(nullptr) == InvalidParam, "a null option block must be rejected");
    Require(sceCompanionHttpdOptParamInitialize(block) == 0, "sceCompanionHttpdOptParamInitialize failed");
    for (std::size_t index = 0; index < sizeof(block); ++index) {
        Require(block[index] == (index < 0x60 ? 0u : 0x5au), "the option block must be zeroed over its 0x60 bytes and nothing past them");
    }
}

void CheckStates() {
    unsigned char param[0x60]{};
    unsigned char event[0x104];
    std::memset(event, 0x5a, sizeof(event));
    unsigned char untouched[0x104];
    std::memset(untouched, 0x5a, sizeof(untouched));
    auto* callback = reinterpret_cast<void*>(&RequestCallback);
    Require(sceCompanionHttpdGetEvent(event) == NotInitialized, "an event before initialization");
    Require(sceCompanionHttpdStart() == NotInitialized, "a start before initialization");
    Require(sceCompanionHttpdStop() == NotInitialized, "a stop before initialization");
    Require(sceCompanionHttpdTerminate() == NotInitialized, "a termination before initialization");
    Require(sceCompanionHttpdRegisterRequestCallback(callback, nullptr) == NotInitialized, "a callback before initialization");
    Require(sceCompanionHttpdUnregisterRequestCallback() == NotInitialized, "an unregistration before initialization");
    Require(sceCompanionHttpdInitialize(nullptr) == InvalidParam, "a null parameter must be rejected");
    Require(sceCompanionHttpdInitialize(param) == 0, "sceCompanionHttpdInitialize failed");
    Require(sceCompanionHttpdInitialize(param) == AlreadyInitialized, "a second initialization");
    Require(sceCompanionHttpdRegisterRequestCallback(nullptr, nullptr) == InvalidParam, "a null callback must be rejected");
    Require(sceCompanionHttpdRegisterRequestCallback(callback, param) == 0, "registering the request callback failed");
    Require(sceCompanionHttpdRegisterRequestCallback(callback, param) == AlreadyRegistered, "a second callback");
    Require(sceCompanionHttpdGetEvent(nullptr) == InvalidParam, "a null event must be rejected");
    Require(sceCompanionHttpdGetEvent(event) == NoEvent, "no companion device connects before the start");
    Require(sceCompanionHttpdStop() == NotStarted, "a stop before the start");
    Require(sceCompanionHttpdStart() == 0, "sceCompanionHttpdStart failed");
    Require(sceCompanionHttpdStart() == AlreadyStarted, "a second start");
    Require(sceCompanionHttpdGetEvent(event) == NoEvent, "no companion device connects after the start");
    Require(std::memcmp(event, untouched, sizeof(event)) == 0, "sceCompanionHttpdGetEvent wrote the event");
    RequireNotImplemented([] { sceCompanionHttpdTerminate(); }, "sceCompanionHttpdTerminate");
    Require(sceCompanionHttpdStop() == 0, "sceCompanionHttpdStop failed");
    Require(sceCompanionHttpdUnregisterRequestCallback() == 0, "unregistering the request callback failed");
    RequireNotImplemented([] { sceCompanionHttpdUnregisterRequestCallback(); }, "sceCompanionHttpdUnregisterRequestCallback");
    Require(sceCompanionHttpdTerminate() == 0, "sceCompanionHttpdTerminate failed");
    Require(sceCompanionHttpdTerminate() == NotInitialized, "a second termination");
    Require(sceCompanionHttpdInitialize(param) == 0, "initializing again after the termination failed");
    Require(sceCompanionHttpdRegisterRequestCallback(callback, nullptr) == 0, "the termination kept the callback");
    Require(sceCompanionHttpdTerminate() == 0, "the last termination failed");
}

void CheckResponseBuilders() {
    unsigned char response[0x20]{};
    RequireNotImplemented([&] { sceCompanionHttpdAddHeader("key", "value", response); }, "sceCompanionHttpdAddHeader");
    RequireNotImplemented([&] { sceCompanionHttpdSetStatus(200, response); }, "sceCompanionHttpdSetStatus");
    RequireNotImplemented([&] { sceCompanionHttpdSetBody("body", 4, response); }, "sceCompanionHttpdSetBody");
    RequireNotImplemented([] { sceCompanionHttpdGet2ndScreenStatus(); }, "sceCompanionHttpdGet2ndScreenStatus");
    RequireNotImplemented([] { sceCompanionHttpdGetUserId(); }, "sceCompanionHttpdGetUserId");
    RequireNotImplemented([] { sceCompanionHttpdInitialize2(); }, "sceCompanionHttpdInitialize2");
    RequireNotImplemented([] { sceCompanionHttpdRegisterRequestBodyReceptionCallback(); }, "sceCompanionHttpdRegisterRequestBodyReceptionCallback");
    RequireNotImplemented([] { sceCompanionHttpdRegisterRequestCallback2(); }, "sceCompanionHttpdRegisterRequestCallback2");
    RequireNotImplemented([] { sceCompanionHttpdUnregisterRequestBodyReceptionCallback(); }, "sceCompanionHttpdUnregisterRequestBodyReceptionCallback");
}

}

int main() {
    CheckOptionBlock();
    CheckStates();
    CheckResponseBuilders();
    return 0;
}
