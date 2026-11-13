#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

const void* APS5_VABI WKContextGetNotificationManager(const void* contextRef) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    if (!pool->notificationManager) {
        pool->notificationManager = NKWebKit::Make<NKWebKit::NotificationManager>();
    }
    return pool->notificationManager.Get();
}

void APS5_VABI WKNotificationManagerSetProvider(const void* managerRef, const WKNotificationProviderBase* provider) {
    auto* manager = NKWebKit::Cast<NKWebKit::NotificationManager>(managerRef, __func__);
    if (provider) {
        manager->provider = *provider;
        manager->hasProvider = true;
    } else {
        manager->provider = {};
        manager->hasProvider = false;
    }
}

void APS5_VABI WKNotificationManagerProviderDidShowNotification(const void* managerRef, std::uint64_t notificationID) {
    auto* manager = NKWebKit::Cast<NKWebKit::NotificationManager>(managerRef, __func__);
    manager->shownNotificationIDs.push_back(notificationID);
}

void APS5_VABI WKNotificationManagerProviderDidCloseNotifications(const void* managerRef, const void* notificationIDsRef) {
    auto* manager = NKWebKit::Cast<NKWebKit::NotificationManager>(managerRef, __func__);
    if (notificationIDsRef) {
        const auto* array = NKWebKit::Cast<NKWebKit::Array>(notificationIDsRef, __func__);
        manager->closedNotificationCounts.push_back(array->elements.size());
    }
}

void APS5_VABI WKNotificationManagerProviderDidClickNotification(const void* managerRef, std::uint64_t notificationID) {
    auto* manager = NKWebKit::Cast<NKWebKit::NotificationManager>(managerRef, __func__);
    manager->clickedNotificationIDs.push_back(notificationID);
}

void APS5_VABI WKNotificationManagerProviderDidUpdateNotificationPolicy(const void* managerRef, const void* originRef, bool allowed) {
    NKWebKit::Cast<NKWebKit::NotificationManager>(managerRef, __func__);
    if (originRef) {
        NKWebKit::ToObject(originRef, __func__);
    }
    (void)allowed;
}

void APS5_VABI WKNotificationManagerProviderDidRemoveNotificationPolicies(const void* managerRef, const void* originsRef) {
    NKWebKit::Cast<NKWebKit::NotificationManager>(managerRef, __func__);
    if (originsRef) {
        NKWebKit::Cast<NKWebKit::Array>(originsRef, __func__);
    }
}

}
