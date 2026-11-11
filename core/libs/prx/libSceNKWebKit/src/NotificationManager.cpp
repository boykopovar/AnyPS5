#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

const void* APS5_VABI WKContextGetNotificationManager(const void* contextRef) {
    const auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    return static_cast<const void*>(pool->notificationManager.Get());
}

void APS5_VABI WKNotificationManagerSetProvider(const void* managerRef, const void* provider) {
    auto* manager = NKWebKit::Cast<NKWebKit::NotificationManager>(managerRef, __func__);
    if (manager->provider.has_value() && manager->provider->removeNotificationManager) {
        manager->provider->removeNotificationManager(managerRef, manager->provider->base.clientInfo);
    }
    if (!provider) {
        manager->provider.reset();
        return;
    }
    const auto* base = static_cast<const NKWebKit::WKNotificationProviderBase*>(provider);
    if (base->version == 0) {
        const auto* v0 = static_cast<const NKWebKit::WKNotificationProviderV0*>(provider);
        manager->provider = *v0;
        if (manager->provider->addNotificationManager) {
            manager->provider->addNotificationManager(managerRef, manager->provider->base.clientInfo);
        }
    } else {
        throw std::invalid_argument("WKNotificationManagerSetProvider: unsupported provider version");
    }
}

void APS5_VABI WKNotificationManagerProviderDidShowNotification(const void* managerRef, std::uint64_t notificationID) {
    auto* manager = NKWebKit::Cast<NKWebKit::NotificationManager>(managerRef, __func__);
    manager->shownNotifications.push_back(notificationID);
}

void APS5_VABI WKNotificationManagerProviderDidCloseNotifications(const void* managerRef, const void* notificationIDsRef) {
    auto* manager = NKWebKit::Cast<NKWebKit::NotificationManager>(managerRef, __func__);
    manager->closedNotificationBatches.push_back(NKWebKit::Retained<NKWebKit::Object>::Share(
        NKWebKit::Cast<NKWebKit::Array>(notificationIDsRef, __func__)));
}

void APS5_VABI WKNotificationManagerProviderDidClickNotification(const void* managerRef, std::uint64_t notificationID) {
    auto* manager = NKWebKit::Cast<NKWebKit::NotificationManager>(managerRef, __func__);
    manager->clickedNotifications.push_back(notificationID);
}

void APS5_VABI WKNotificationManagerProviderDidUpdateNotificationPolicy(const void* managerRef, const void* originRef, bool allowed) {
    auto* manager = NKWebKit::Cast<NKWebKit::NotificationManager>(managerRef, __func__);
    manager->originPolicies.push_back({
        NKWebKit::Retained<NKWebKit::Object>::Share(
            NKWebKit::Cast<NKWebKit::ObjectOf<NKWebKit::ObjectType::SecurityOrigin>>(originRef, __func__)),
        allowed
    });
}

void APS5_VABI WKNotificationManagerProviderDidRemoveNotificationPolicies(const void* managerRef, const void* originsRef) {
    auto* manager = NKWebKit::Cast<NKWebKit::NotificationManager>(managerRef, __func__);
    manager->removedPolicyBatches.push_back(NKWebKit::Retained<NKWebKit::Object>::Share(
        NKWebKit::Cast<NKWebKit::Array>(originsRef, __func__)));
}


}
