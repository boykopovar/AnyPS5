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

void RequireMissingBridge(void (*call)(), const char* name) {
    const std::string expected = std::string(name) + " not implemented";
    try {
        call();
    } catch (const std::runtime_error& error) {
        Require(error.what() == expected);
        return;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s reported an unexpected error: %s\n", name, error.what());
        std::exit(1);
    }
    std::fprintf(stderr, "%s did not report a missing bridge\n", name);
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

void CallResizeStringBuilderWithoutBridge() {
    Jsc::WtfAbi::StringBuilder builder{};
    _ZN3WTF13StringBuilder6resizeEj(&builder, 1);
}

void TestMissingBridgeReportsNotImplemented() {
    RegisterApiBridge_nid_no_patch(nullptr);

    RequireMissingBridge(&CallCreateWithoutBridge, "JSWeakObjectMapCreate");
    RequireMissingBridge(&CallMakeStringWithoutBridge, "JSValueMakeString");
    RequireMissingBridge(&CallHasPropertyWithoutBridge, "JSObjectHasPropertyForKey");
    RequireMissingBridge(&CallResizeStringBuilderWithoutBridge, "_ZN3WTF13StringBuilder6resizeEj");
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
    TestMissingBridgeReportsNotImplemented();
    TestNullContextPathsDoNotNeedABridge();
    RegisterApiBridge_nid_no_patch(nullptr);
    std::puts("libSceJsc JSWeakObjectMapCreate tests passed");
    return 0;
}
