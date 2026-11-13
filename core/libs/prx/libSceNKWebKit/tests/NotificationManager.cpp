#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdint>
#include <functional>
#include <stdexcept>

namespace {

const void* gLastAddedManager = nullptr;
const void* gLastAddedClientInfo = nullptr;
const void* gLastRemovedManager = nullptr;
const void* gLastRemovedClientInfo = nullptr;

void APS5_VABI RecordAddedManager(const void* manager, const void* clientInfo) {
    gLastAddedManager = manager;
    gLastAddedClientInfo = clientInfo;
}

void APS5_VABI RecordRemovedManager(const void* manager, const void* clientInfo) {
    gLastRemovedManager = manager;
    gLastRemovedClientInfo = clientInfo;
}

void Require(bool condition) {
    if (!condition) {
        throw std::runtime_error("Assertion failed");
    }
}

void RequireInvalidArgument(const std::function<void()>& action) {
    bool threw = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    Require(WKNotificationManagerGetTypeID() == static_cast<std::uint32_t>(NKWebKit::ObjectType::NotificationManager));

    auto pool = NKWebKit::Make<NKWebKit::ProcessPool>();
    const void* contextRef = NKWebKit::ToRef(pool);

    const void* managerRef = WKContextGetNotificationManager(contextRef);
    Require(managerRef != nullptr);
    Require(WKContextGetNotificationManager(contextRef) == managerRef);

    auto* manager = NKWebKit::Cast<NKWebKit::NotificationManager>(managerRef, "test");
    Require(!manager->provider.has_value());
    Require(manager->shownNotifications.empty());
    Require(manager->clickedNotifications.empty());
    Require(manager->closedNotificationBatches.empty());
    Require(manager->originPolicies.empty());
    Require(manager->removedPolicyBatches.empty());

    int dummyClientInfo1 = 123;
    NKWebKit::WKNotificationProviderV0 provider1{};
    provider1.base.version = 0;
    provider1.base.clientInfo = &dummyClientInfo1;
    provider1.addNotificationManager = RecordAddedManager;
    provider1.removeNotificationManager = RecordRemovedManager;

    WKNotificationManagerSetProvider(managerRef, &provider1.base);
    Require(manager->provider.has_value());
    Require(gLastAddedManager == managerRef);
    Require(gLastAddedClientInfo == &dummyClientInfo1);
    Require(gLastRemovedManager == nullptr);

    int dummyClientInfo2 = 456;
    NKWebKit::WKNotificationProviderV0 provider2{};
    provider2.base.version = 0;
    provider2.base.clientInfo = &dummyClientInfo2;
    provider2.addNotificationManager = RecordAddedManager;
    provider2.removeNotificationManager = RecordRemovedManager;

    WKNotificationManagerSetProvider(managerRef, &provider2.base);
    Require(manager->provider.has_value());
    Require(gLastRemovedManager == managerRef);
    Require(gLastRemovedClientInfo == &dummyClientInfo1);
    Require(gLastAddedManager == managerRef);
    Require(gLastAddedClientInfo == &dummyClientInfo2);

    WKNotificationManagerSetProvider(managerRef, nullptr);
    Require(!manager->provider.has_value());
    Require(gLastRemovedManager == managerRef);
    Require(gLastRemovedClientInfo == &dummyClientInfo2);

    NKWebKit::WKNotificationProviderBase invalidProvider{};
    invalidProvider.version = 999;
    RequireInvalidArgument([&] { WKNotificationManagerSetProvider(managerRef, &invalidProvider); });

    WKNotificationManagerProviderDidShowNotification(managerRef, 1001ULL);
    WKNotificationManagerProviderDidShowNotification(managerRef, 2002ULL);
    Require(manager->shownNotifications.size() == 2);
    Require(manager->shownNotifications[0] == 1001ULL);
    Require(manager->shownNotifications[1] == 2002ULL);

    WKNotificationManagerProviderDidClickNotification(managerRef, 1001ULL);
    Require(manager->clickedNotifications.size() == 1);
    Require(manager->clickedNotifications[0] == 1001ULL);

    auto closedArray = NKWebKit::Make<NKWebKit::Array>();
    const void* closedArrayRef = NKWebKit::ToRef(closedArray);
    WKNotificationManagerProviderDidCloseNotifications(managerRef, closedArrayRef);
    Require(manager->closedNotificationBatches.size() == 1);
    Require(manager->closedNotificationBatches[0].Get() == closedArray.Get());

    auto origin = NKWebKit::Make<NKWebKit::ObjectOf<NKWebKit::ObjectType::SecurityOrigin>>();
    const void* originRef = NKWebKit::ToRef(origin);
    WKNotificationManagerProviderDidUpdateNotificationPolicy(managerRef, originRef, true);
    Require(manager->originPolicies.size() == 1);
    Require(manager->originPolicies[0].first.Get() == origin.Get());
    Require(manager->originPolicies[0].second == true);

    WKNotificationManagerProviderDidUpdateNotificationPolicy(managerRef, originRef, false);
    Require(manager->originPolicies.size() == 2);
    Require(manager->originPolicies[1].first.Get() == origin.Get());
    Require(manager->originPolicies[1].second == false);

    auto removeArray = NKWebKit::Make<NKWebKit::Array>();
    const void* removeArrayRef = NKWebKit::ToRef(removeArray);
    WKNotificationManagerProviderDidRemoveNotificationPolicies(managerRef, removeArrayRef);
    Require(manager->removedPolicyBatches.size() == 1);
    Require(manager->removedPolicyBatches[0].Get() == removeArray.Get());

    RequireInvalidArgument([] { WKContextGetNotificationManager(nullptr); });
    RequireInvalidArgument([] { WKNotificationManagerSetProvider(nullptr, nullptr); });
    RequireInvalidArgument([] { WKNotificationManagerProviderDidShowNotification(nullptr, 1ULL); });
    RequireInvalidArgument([&] { WKNotificationManagerProviderDidCloseNotifications(nullptr, closedArrayRef); });
    RequireInvalidArgument([&] { WKNotificationManagerProviderDidCloseNotifications(managerRef, nullptr); });
    RequireInvalidArgument([] { WKNotificationManagerProviderDidClickNotification(nullptr, 1ULL); });
    RequireInvalidArgument([&] { WKNotificationManagerProviderDidUpdateNotificationPolicy(nullptr, originRef, true); });
    RequireInvalidArgument([&] { WKNotificationManagerProviderDidUpdateNotificationPolicy(managerRef, nullptr, true); });
    RequireInvalidArgument([&] { WKNotificationManagerProviderDidRemoveNotificationPolicies(nullptr, removeArrayRef); });
    RequireInvalidArgument([&] { WKNotificationManagerProviderDidRemoveNotificationPolicies(managerRef, nullptr); });

    RequireInvalidArgument([&] { WKNotificationManagerSetProvider(contextRef, nullptr); });
    RequireInvalidArgument([&] { WKNotificationManagerProviderDidCloseNotifications(managerRef, originRef); });
    RequireInvalidArgument([&] { WKNotificationManagerProviderDidUpdateNotificationPolicy(managerRef, closedArrayRef, true); });
    RequireInvalidArgument([&] { WKNotificationManagerProviderDidRemoveNotificationPolicies(managerRef, originRef); });

    return 0;
}
