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

namespace {

struct NotificationProviderBase {
    int version = 0;
    const void* clientInfo = nullptr;
};

using NotificationProviderAddManager = void (APS5_VABI*)(const void* manager, const void* clientInfo);
using NotificationProviderRemoveManager = void (APS5_VABI*)(const void* manager, const void* clientInfo);

struct NotificationProviderV0 {
    NotificationProviderBase base;
    const void* show = nullptr;
    const void* cancel = nullptr;
    const void* didDestroyNotification = nullptr;
    NotificationProviderAddManager addNotificationManager = nullptr;
    NotificationProviderRemoveManager removeNotificationManager = nullptr;
    const void* notificationPermissions = nullptr;
    const void* clearNotifications = nullptr;
};

struct ProviderTrackingState {
    const void* addedManager = nullptr;
    const void* addedClientInfo = nullptr;
    const void* removedManager = nullptr;
    const void* removedClientInfo = nullptr;
    int addCount = 0;
    int removeCount = 0;
};

void APS5_VABI TestAddManager(const void* manager, const void* clientInfo) {
    auto* state = static_cast<ProviderTrackingState*>(const_cast<void*>(clientInfo));
    state->addedManager = manager;
    state->addedClientInfo = clientInfo;
    state->addCount++;
}

void APS5_VABI TestRemoveManager(const void* manager, const void* clientInfo) {
    auto* state = static_cast<ProviderTrackingState*>(const_cast<void*>(clientInfo));
    state->removedManager = manager;
    state->removedClientInfo = clientInfo;
    state->removeCount++;
}

}

int main() {
    const void* contextRef = WKContextCreate();
    Require(contextRef != nullptr);
    Require(WKGetTypeID(contextRef) == WKContextGetTypeID());

    const void* managerRef = WKContextGetNotificationManager(contextRef);
    Require(managerRef != nullptr);
    Require(WKGetTypeID(managerRef) == WKNotificationManagerGetTypeID());

    const void* managerRef2 = WKContextGetNotificationManager(contextRef);
    Require(managerRef == managerRef2);

    auto* manager = NKWebKit::Cast<NKWebKit::NotificationManager>(managerRef, __func__);

    ProviderTrackingState trackingState;
    NotificationProviderV0 providerV0;
    providerV0.base.version = 0;
    providerV0.base.clientInfo = &trackingState;
    providerV0.addNotificationManager = TestAddManager;
    providerV0.removeNotificationManager = TestRemoveManager;

    WKNotificationManagerSetProvider(managerRef, &providerV0);
    Require(manager->provider == &providerV0);
    Require(trackingState.addedManager == managerRef);
    Require(trackingState.addedClientInfo == &trackingState);
    Require(trackingState.addCount == 1);
    Require(trackingState.removeCount == 0);

    WKNotificationManagerSetProvider(managerRef, &providerV0);
    Require(manager->provider == &providerV0);
    Require(trackingState.addCount == 1);
    Require(trackingState.removeCount == 0);

    WKNotificationManagerSetProvider(managerRef, nullptr);
    Require(manager->provider == nullptr);
    Require(trackingState.removedManager == managerRef);
    Require(trackingState.removedClientInfo == &trackingState);
    Require(trackingState.addCount == 1);
    Require(trackingState.removeCount == 1);

    int dummyProvider = 42;
    WKNotificationManagerSetProvider(managerRef, &dummyProvider);
    Require(manager->provider == &dummyProvider);

    WKNotificationManagerProviderDidShowNotification(managerRef, 1001);
    WKNotificationManagerProviderDidShowNotification(managerRef, 1002);
    Require(manager->shownNotifications.count(1001) == 1);
    Require(manager->shownNotifications.count(1002) == 1);
    Require(manager->shownNotifications.count(1003) == 0);

    WKNotificationManagerProviderDidClickNotification(managerRef, 1001);
    Require(manager->clickedNotifications.count(1001) == 1);
    Require(manager->clickedNotifications.count(1002) == 0);

    const void* id1 = WKUInt64Create(1001);
    const void* id2 = WKUInt64Create(2002);
    const void* closeItems[] = { id1, id2 };
    const void* closeArray = WKArrayCreate(closeItems, 2);

    WKNotificationManagerProviderDidCloseNotifications(managerRef, closeArray);
    Require(manager->closedNotifications.count(1001) == 1);
    Require(manager->closedNotifications.count(2002) == 1);
    Require(manager->closedNotifications.count(3003) == 0);

    const void* originStr1 = WKStringCreateWithUTF8CString("https://example.com");
    const void* originStr2 = WKStringCreateWithUTF8CString("https://sony.com");
    auto originWithPort1 = NKWebKit::Make<NKWebKit::SecurityOrigin>(NKWebKit::String(std::u16string(u"https")), NKWebKit::String(std::u16string(u"example.com")), std::optional<std::uint16_t>(8443));
    auto originWithPort2 = NKWebKit::Make<NKWebKit::SecurityOrigin>(NKWebKit::String(std::u16string(u"http")), NKWebKit::String(std::u16string(u"example.com")), std::optional<std::uint16_t>(8080));
    const void* originRef1 = NKWebKit::ToRef(originWithPort1);
    const void* originRef2 = NKWebKit::ToRef(originWithPort2);
    const void* originItems[] = { originStr1, originStr2, originRef1, originRef2 };
    const void* originArray = WKArrayCreate(originItems, 4);

    WKNotificationManagerProviderDidRemoveNotificationPolicies(managerRef, originArray);
    Require(manager->removedNotificationPolicies.size() == 4);
    Require(manager->removedNotificationPolicies[0] == "https://example.com");
    Require(manager->removedNotificationPolicies[1] == "https://sony.com");
    Require(manager->removedNotificationPolicies[2] == "https://example.com:8443");
    Require(manager->removedNotificationPolicies[3] == "http://example.com:8080");

    const void* dummyObj = WKStringCreateWithUTF8CString("dummy");

    RequireThrows([&] { WKContextGetNotificationManager(nullptr); });
    RequireThrows([&] { WKContextGetNotificationManager(dummyObj); });

    RequireThrows([&] { WKNotificationManagerSetProvider(nullptr, &dummyProvider); });
    RequireThrows([&] { WKNotificationManagerSetProvider(dummyObj, &dummyProvider); });

    RequireThrows([&] { WKNotificationManagerProviderDidShowNotification(nullptr, 1); });
    RequireThrows([&] { WKNotificationManagerProviderDidShowNotification(dummyObj, 1); });

    RequireThrows([&] { WKNotificationManagerProviderDidCloseNotifications(nullptr, closeArray); });
    RequireThrows([&] { WKNotificationManagerProviderDidCloseNotifications(dummyObj, closeArray); });
    RequireThrows([&] { WKNotificationManagerProviderDidCloseNotifications(managerRef, nullptr); });
    RequireThrows([&] { WKNotificationManagerProviderDidCloseNotifications(managerRef, dummyObj); });

    RequireThrows([&] { WKNotificationManagerProviderDidClickNotification(nullptr, 1); });
    RequireThrows([&] { WKNotificationManagerProviderDidClickNotification(dummyObj, 1); });

    RequireThrows([&] { WKNotificationManagerProviderDidRemoveNotificationPolicies(nullptr, originArray); });
    RequireThrows([&] { WKNotificationManagerProviderDidRemoveNotificationPolicies(dummyObj, originArray); });
    RequireThrows([&] { WKNotificationManagerProviderDidRemoveNotificationPolicies(managerRef, nullptr); });
    RequireThrows([&] { WKNotificationManagerProviderDidRemoveNotificationPolicies(managerRef, dummyObj); });

    WKRelease(id1);
    WKRelease(id2);
    WKRelease(closeArray);
    WKRelease(originStr1);
    WKRelease(originStr2);
    WKRelease(originRef1);
    WKRelease(originRef2);
    WKRelease(originArray);
    WKRelease(dummyObj);
    WKRelease(contextRef);

    ProviderTrackingState teardownTracking;
    NotificationProviderV0 teardownProviderV0;
    teardownProviderV0.base.version = 0;
    teardownProviderV0.base.clientInfo = &teardownTracking;
    teardownProviderV0.addNotificationManager = TestAddManager;
    teardownProviderV0.removeNotificationManager = TestRemoveManager;

    const void* teardownContextRef = WKContextCreate();
    const void* teardownManagerRef = WKContextGetNotificationManager(teardownContextRef);
    WKNotificationManagerSetProvider(teardownManagerRef, &teardownProviderV0);
    Require(teardownTracking.addCount == 1);
    Require(teardownTracking.removeCount == 0);

    WKRelease(teardownContextRef);
    Require(teardownTracking.removeCount == 1);
    Require(teardownTracking.removedManager == teardownManagerRef);
    Require(teardownTracking.removedClientInfo == &teardownTracking);

    return 0;
}
