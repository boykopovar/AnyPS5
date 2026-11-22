#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<typename Fn>
static void RequireThrows(Fn&& fn) {
    bool threw = false;
    try {
        fn();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

int main() {
    const void* contextRef = WKContextCreate();
    Require(contextRef != nullptr);
    const auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);

    Require(pool->injectedBundleInitializationUserData.Get() == nullptr);
    Require(pool->lastInjectedBundleMessageName.Get() == nullptr);
    Require(pool->lastInjectedBundleMessageBody.Get() == nullptr);
    Require(!pool->injectedBundleClientConfigured);

    const void* userData = WKStringCreateWithUTF8CString("custom_user_data");
    WKContextSetInitializationUserDataForInjectedBundle(contextRef, userData);
    Require(pool->injectedBundleInitializationUserData.Get() != nullptr);

    WKContextSetInitializationUserDataForInjectedBundle(contextRef, nullptr);
    Require(pool->injectedBundleInitializationUserData.Get() == nullptr);

    const void* msgName = WKStringCreateWithUTF8CString("PingBundle");
    const void* msgBody = WKStringCreateWithUTF8CString("Payload");
    WKContextPostMessageToInjectedBundle(contextRef, msgName, msgBody);
    Require(pool->lastInjectedBundleMessageName.Get() != nullptr);
    Require(pool->lastInjectedBundleMessageBody.Get() != nullptr);

    const void* msgName2 = WKStringCreateWithUTF8CString("NoPayload");
    WKContextPostMessageToInjectedBundle(contextRef, msgName2, nullptr);
    Require(pool->lastInjectedBundleMessageName.Get() != nullptr);
    Require(pool->lastInjectedBundleMessageBody.Get() == nullptr);

    int dummyClient = 42;
    WKContextSetInjectedBundleClient(contextRef, &dummyClient);
    Require(pool->injectedBundleClientConfigured);

    WKContextSetInjectedBundleClient(contextRef, nullptr);
    Require(!pool->injectedBundleClientConfigured);

    const void* wrongType = WKStringCreateWithUTF8CString("not_a_context");

    RequireThrows([&] { WKContextPostMessageToInjectedBundle(nullptr, msgName, msgBody); });
    RequireThrows([&] { WKContextPostMessageToInjectedBundle(wrongType, msgName, msgBody); });
    RequireThrows([&] { WKContextPostMessageToInjectedBundle(contextRef, nullptr, msgBody); });
    RequireThrows([&] { WKContextSetInitializationUserDataForInjectedBundle(nullptr, userData); });
    RequireThrows([&] { WKContextSetInitializationUserDataForInjectedBundle(wrongType, userData); });
    RequireThrows([&] { WKContextSetInjectedBundleClient(nullptr, &dummyClient); });
    RequireThrows([&] { WKContextSetInjectedBundleClient(wrongType, &dummyClient); });

    WKRelease(wrongType);
    WKRelease(msgName2);
    WKRelease(msgBody);
    WKRelease(msgName);
    WKRelease(userData);
    WKRelease(contextRef);
    return 0;
}
