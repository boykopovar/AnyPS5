#include "prx/libSceJsc/include/ApiBridgeExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "JSWeakObjectMapCreate check failed at line %d\n", line);
        std::exit(1);
    }
}
#define Require(value) Check((value), __LINE__)

char sentinelMap = 0;

const void* lastContext = nullptr;
void* lastPrivateData = nullptr;
Jsc::WeakMapDestroyedCallback lastCallback = nullptr;
unsigned createCalls = 0;

void APS5_VABI Destroyed(void*, void*) {}

const void* UnusedMakeString(const void*, const GuestJSString*) {
    return nullptr;
}

bool UnusedHasPropertyForKey(const void*, const void*, const void*, const void**) {
    return false;
}

void UnusedResizeStringBuilder(Jsc::WtfAbi::StringBuilder*, unsigned) {}

void* CreateWeakObjectMap(const void* context, void* privateData, Jsc::WeakMapDestroyedCallback callback) {
    ++createCalls;
    lastContext = context;
    lastPrivateData = privateData;
    lastCallback = callback;
    return &sentinelMap;
}

const Jsc::ApiBridge bridge{&UnusedMakeString, &UnusedHasPropertyForKey, &UnusedResizeStringBuilder, &CreateWeakObjectMap};

std::string LogicErrorOf(void (*call)(), const char* name) {
    try {
        call();
    } catch (const std::logic_error& error) {
        return error.what();
    }
    std::fprintf(stderr, "%s did not throw a logic error\n", name);
    std::exit(1);
}

void TestNullContextThrowsBeforeTheBridge() {
    RegisterApiBridge_nid_no_patch(&bridge);
    createCalls = 0;
    lastContext = nullptr;

    std::string message;
    try {
        JSWeakObjectMapCreate(nullptr, nullptr, &Destroyed);
    } catch (const std::invalid_argument& error) {
        message = error.what();
    }
    Require(message.find("JSWeakObjectMapCreate") != std::string::npos);
    Require(createCalls == 0);
    Require(lastContext == nullptr);
}

void TestCreateForwardsItsArguments() {
    RegisterApiBridge_nid_no_patch(&bridge);
    createCalls = 0;

    char context = 0;
    int privateData = 3;
    void* map = JSWeakObjectMapCreate(&context, &privateData, &Destroyed);

    Require(map == &sentinelMap);
    Require(createCalls == 1);
    Require(lastContext == &context);
    Require(lastPrivateData == &privateData);
    Require(lastCallback == &Destroyed);
}

void TestNullPrivateDataAndDestructorAreForwarded() {
    RegisterApiBridge_nid_no_patch(&bridge);
    createCalls = 0;
    lastPrivateData = &createCalls;
    lastCallback = &Destroyed;

    char context = 0;
    void* map = JSWeakObjectMapCreate(&context, nullptr, nullptr);

    Require(map == &sentinelMap);
    Require(createCalls == 1);
    Require(lastPrivateData == nullptr);
    Require(lastCallback == nullptr);
}

char bridgeContext = 0;

void CallCreateWithoutBridge() {
    JSWeakObjectMapCreate(&bridgeContext, nullptr, nullptr);
}

void CallMakeStringWithoutBridge() {
    JSValueMakeString(&bridgeContext, nullptr);
}

void CallHasPropertyWithoutBridge() {
    JSObjectHasPropertyForKey(&bridgeContext, &bridgeContext, &bridgeContext, nullptr);
}

void TestMissingBridgeThrowsALogicError() {
    RegisterApiBridge_nid_no_patch(nullptr);

    const std::string create = LogicErrorOf(&CallCreateWithoutBridge, "JSWeakObjectMapCreate");
    Require(create.find("JSWeakObjectMapCreate") != std::string::npos);
    Require(create.find("no JavaScriptCore bridge is registered") != std::string::npos);
    Require(LogicErrorOf(&CallMakeStringWithoutBridge, "JSValueMakeString").find("JSValueMakeString") != std::string::npos);
    Require(LogicErrorOf(&CallHasPropertyWithoutBridge, "JSObjectHasPropertyForKey").find("JSObjectHasPropertyForKey") != std::string::npos);
}

void TestNullContextPathsDoNotNeedABridge() {
    RegisterApiBridge_nid_no_patch(nullptr);
    Require(JSValueMakeString(nullptr, nullptr) == nullptr);
    Require(!JSObjectHasPropertyForKey(nullptr, nullptr, nullptr, nullptr));

    bool threw = false;
    try {
        JSWeakObjectMapCreate(nullptr, nullptr, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    TestNullContextThrowsBeforeTheBridge();
    TestCreateForwardsItsArguments();
    TestNullPrivateDataAndDestructorAreForwarded();
    TestMissingBridgeThrowsALogicError();
    TestNullContextPathsDoNotNeedABridge();
    RegisterApiBridge_nid_no_patch(nullptr);
    std::puts("libSceJsc JSWeakObjectMapCreate tests passed");
    return 0;
}
