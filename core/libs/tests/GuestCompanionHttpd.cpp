#include "prx/libSceCompanionHttpd/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int InvalidParam = static_cast<int>(0x80E40004u);
constexpr int NoEvent = static_cast<int>(0x80E40008u);

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "CompanionHttpd: %s\n", message);
        std::abort();
    }
}

int APS5_VABI RequestCallback(int, int, const void*, void*, void*) {
    std::fprintf(stderr, "CompanionHttpd: the request callback was called\n");
    std::abort();
}

void CheckOptionBlock() {
    unsigned char block[0x70];
    std::memset(block, 0x5a, sizeof(block));
    Require(sceCompanionHttpdOptParamInitialize(nullptr) == InvalidParam, "a null option block must be rejected");
    Require(sceCompanionHttpdOptParamInitialize(reinterpret_cast<CompanionHttpdOptParam*>(0x10)) == InvalidParam, "an unwritable option block must return InvalidParam");
    Require(sceCompanionHttpdOptParamInitialize(reinterpret_cast<CompanionHttpdOptParam*>(block)) == 0, "sceCompanionHttpdOptParamInitialize failed");
    for (std::size_t index = 0; index < sizeof(block); ++index) {
        Require(block[index] == (index < 0x60 ? 0u : 0x5au), "the option block must be zeroed over its 0x60 bytes and nothing past them");
    }
}

void RequireSecondaryCallsReturnZero(const char* message) {
    Require(sceCompanionHttpdGet2ndScreenStatus() == 0, message);
    Require(sceCompanionHttpdGetUserId() == 0, message);
    Require(sceCompanionHttpdInitialize2() == 0, message);
    Require(sceCompanionHttpdRegisterRequestBodyReceptionCallback() == 0, message);
    Require(sceCompanionHttpdRegisterRequestCallback2() == 0, message);
    Require(sceCompanionHttpdUnregisterRequestBodyReceptionCallback() == 0, message);
}

void RequireNoEvent(unsigned char* event, const unsigned char* untouched, std::size_t size, const char* message) {
    Require(sceCompanionHttpdGetEvent(nullptr) == NoEvent, message);
    Require(sceCompanionHttpdGetEvent(event) == NoEvent, message);
    Require(std::memcmp(event, untouched, size) == 0, "sceCompanionHttpdGetEvent wrote the event");
}

void CheckStates() {
    unsigned char param[0x60]{};
    unsigned char event[0x104];
    std::memset(event, 0x5a, sizeof(event));
    unsigned char untouched[0x104];
    std::memset(untouched, 0x5a, sizeof(untouched));
    const auto callback = reinterpret_cast<CompanionHttpdRequestCallback>(reinterpret_cast<void*>(&RequestCallback));
    const auto* option = reinterpret_cast<const CompanionHttpdOptParam*>(param);
    RequireNoEvent(event, untouched, sizeof(event), "GetEvent must report no event before initialization");
    Require(sceCompanionHttpdStart() == 0, "a start before initialization must return 0");
    Require(sceCompanionHttpdStop() == 0, "a stop before initialization must return 0");
    Require(sceCompanionHttpdTerminate() == 0, "a termination before initialization must return 0");
    Require(sceCompanionHttpdRegisterRequestCallback(nullptr, nullptr) == 0, "a null callback before initialization must return 0");
    Require(sceCompanionHttpdRegisterRequestCallback(callback, nullptr) == 0, "a callback before initialization must return 0");
    Require(sceCompanionHttpdUnregisterRequestCallback() == 0, "an unregistration before initialization must return 0");
    RequireSecondaryCallsReturnZero("a secondary call before initialization must return 0");
    Require(sceCompanionHttpdInitialize(nullptr) == 0, "a null parameter must return 0");
    Require(sceCompanionHttpdInitialize(option) == 0, "sceCompanionHttpdInitialize failed");
    Require(sceCompanionHttpdInitialize(option) == 0, "a second initialization must return 0");
    RequireSecondaryCallsReturnZero("a secondary call while initialized must return 0");
    Require(sceCompanionHttpdRegisterRequestCallback(nullptr, nullptr) == 0, "a null callback must return 0");
    Require(sceCompanionHttpdRegisterRequestCallback(callback, param) == 0, "registering the request callback failed");
    Require(sceCompanionHttpdRegisterRequestCallback(callback, param) == 0, "a second callback must return 0");
    RequireNoEvent(event, untouched, sizeof(event), "GetEvent must report no event before the start");
    Require(sceCompanionHttpdStop() == 0, "a stop before the start must return 0");
    Require(sceCompanionHttpdStart() == 0, "sceCompanionHttpdStart failed");
    Require(sceCompanionHttpdStart() == 0, "a second start must return 0");
    RequireNoEvent(event, untouched, sizeof(event), "GetEvent must report no event after the start");
    Require(sceCompanionHttpdTerminate() == 0, "a termination while started must return 0");
    Require(sceCompanionHttpdStop() == 0, "sceCompanionHttpdStop failed");
    Require(sceCompanionHttpdUnregisterRequestCallback() == 0, "unregistering the request callback failed");
    Require(sceCompanionHttpdUnregisterRequestCallback() == 0, "an unregistration with no callback must return 0");
    Require(sceCompanionHttpdTerminate() == 0, "sceCompanionHttpdTerminate failed");
    Require(sceCompanionHttpdTerminate() == 0, "a second termination must return 0");
    RequireNoEvent(event, untouched, sizeof(event), "GetEvent must report no event after the termination");
    RequireSecondaryCallsReturnZero("a secondary call after the termination must return 0");
}

void CheckResponseBuilders() {
    unsigned char response[0x20];
    unsigned char untouched[sizeof(response)];
    std::memset(response, 0xa5, sizeof(response));
    std::memcpy(untouched, response, sizeof(response));
    Require(sceCompanionHttpdAddHeader("key", "value", response) == 0, "sceCompanionHttpdAddHeader did not return zero");
    Require(std::memcmp(response, untouched, sizeof(response)) == 0, "sceCompanionHttpdAddHeader wrote the response");
    Require(sceCompanionHttpdSetStatus(200, response) == 0, "sceCompanionHttpdSetStatus did not return zero");
    Require(std::memcmp(response, untouched, sizeof(response)) == 0, "sceCompanionHttpdSetStatus wrote the response");
    Require(sceCompanionHttpdSetBody("body", 4, response) == 0, "sceCompanionHttpdSetBody did not return zero");
    Require(std::memcmp(response, untouched, sizeof(response)) == 0, "sceCompanionHttpdSetBody wrote the response");
}

}

int main() {
    CheckOptionBlock();
    CheckStates();
    CheckResponseBuilders();
    return 0;
}
