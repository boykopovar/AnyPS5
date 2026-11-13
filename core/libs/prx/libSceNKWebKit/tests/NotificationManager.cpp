#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<typename Fn>
void RequireThrows(Fn&& fn) {
    bool threw = false;
    try {
        fn();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    const void* contextRef = WKContextCreate();
    Require(contextRef != nullptr);

    const void* notifManager = WKContextGetNotificationManager(contextRef);
    Require(notifManager != nullptr);
    Require(WKNotificationManagerGetTypeID() == NKWebKit::TypeID(NKWebKit::ObjectType::NotificationManager));

    const auto* manager = NKWebKit::Cast<NKWebKit::NotificationManager>(notifManager, __func__);
    Require(!manager->hasProvider);

    WKNotificationProviderBase provider{};
    provider.version = 1;
    WKNotificationManagerSetProvider(notifManager, &provider);
    Require(manager->hasProvider);

    WKNotificationManagerProviderDidShowNotification(notifManager, 101);
    Require(manager->shownNotificationIDs.size() == 1);
    Require(manager->shownNotificationIDs[0] == 101);

    WKNotificationManagerProviderDidClickNotification(notifManager, 101);
    Require(manager->clickedNotificationIDs.size() == 1);
    Require(manager->clickedNotificationIDs[0] == 101);

    auto array = NKWebKit::Make<NKWebKit::Array>();
    WKNotificationManagerProviderDidCloseNotifications(notifManager, NKWebKit::ToRef(array));
    Require(manager->closedNotificationCounts.size() == 1);

    WKNotificationManagerProviderDidUpdateNotificationPolicy(notifManager, nullptr, true);
    WKNotificationManagerProviderDidRemoveNotificationPolicies(notifManager, nullptr);

    WKNotificationManagerSetProvider(notifManager, nullptr);
    Require(!manager->hasProvider);

    RequireThrows([]() {
        WKContextGetNotificationManager(nullptr);
    });

    RequireThrows([]() {
        WKNotificationManagerSetProvider(nullptr, nullptr);
    });

    return 0;
}
