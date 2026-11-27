#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <string>

extern "C" {

const void* APS5_VABI WKContextGetNotificationManager(const void* contextRef) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    if (!pool->notificationManager) {
        pool->notificationManager = NKWebKit::Make<NKWebKit::NotificationManager>();
    }
    return pool->notificationManager.Get();
}

void APS5_VABI WKNotificationManagerSetProvider(const void* notificationManagerRef, const void* providerRef) {
    auto* manager = NKWebKit::Cast<NKWebKit::NotificationManager>(notificationManagerRef, __func__);
    if (providerRef == manager->provider) {
        return;
    }
    manager->teardown();
    manager->provider = providerRef;
    if (providerRef) {
        const auto* newBase = static_cast<const NKWebKit::NotificationProviderBase*>(providerRef);
        if (newBase->version == 0) {
            const auto* newV0 = static_cast<const NKWebKit::NotificationProviderV0*>(providerRef);
            manager->providerV0 = *newV0;
            if (manager->providerV0->addNotificationManager) {
                manager->providerV0->addNotificationManager(notificationManagerRef, manager->providerV0->base.clientInfo);
            }
        }
    }
}

void APS5_VABI WKNotificationManagerProviderDidShowNotification(const void* notificationManagerRef, std::uint64_t notificationID) {
    auto* manager = NKWebKit::Cast<NKWebKit::NotificationManager>(notificationManagerRef, __func__);
    manager->shownNotifications.insert(notificationID);
}

void APS5_VABI WKNotificationManagerProviderDidCloseNotifications(const void* notificationManagerRef, const void* notificationIDsRef) {
    auto* manager = NKWebKit::Cast<NKWebKit::NotificationManager>(notificationManagerRef, __func__);
    const auto* array = NKWebKit::Cast<NKWebKit::Array>(notificationIDsRef, __func__);
    for (const auto& item : array->elements) {
        if (!item) {
            continue;
        }
        if (item->Type() == NKWebKit::ObjectType::UInt64) {
            manager->closedNotifications.insert(static_cast<const NKWebKit::UInt64*>(item.Get())->value);
        } else if (item->Type() == NKWebKit::ObjectType::Notification) {
            manager->closedNotifications.insert(static_cast<const NKWebKit::Notification*>(item.Get())->notificationID);
        }
    }
}

void APS5_VABI WKNotificationManagerProviderDidClickNotification(const void* notificationManagerRef, std::uint64_t notificationID) {
    auto* manager = NKWebKit::Cast<NKWebKit::NotificationManager>(notificationManagerRef, __func__);
    manager->clickedNotifications.insert(notificationID);
}

void APS5_VABI WKNotificationManagerProviderDidRemoveNotificationPolicies(const void* notificationManagerRef, const void* originsRef) {
    auto* manager = NKWebKit::Cast<NKWebKit::NotificationManager>(notificationManagerRef, __func__);
    const auto* array = NKWebKit::Cast<NKWebKit::Array>(originsRef, __func__);
    for (const auto& item : array->elements) {
        if (!item) {
            continue;
        }
        if (item->Type() == NKWebKit::ObjectType::SecurityOrigin) {
            const auto* origin = static_cast<const NKWebKit::SecurityOrigin*>(item.Get());
            std::string serialized;
            if (!origin->protocol.empty()) {
                serialized += NKWebKit::EncodeUtf8(origin->protocol) + "://";
            }
            serialized += NKWebKit::EncodeUtf8(origin->host);
            if (origin->port.has_value() && origin->port.value() != 0) {
                serialized += ":" + std::to_string(origin->port.value());
            }
            manager->removedNotificationPolicies.push_back(serialized);
        } else if (item->Type() == NKWebKit::ObjectType::String) {
            const auto* str = static_cast<const NKWebKit::String*>(item.Get());
            manager->removedNotificationPolicies.push_back(str->Utf8());
        }
    }
}

}
