#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <utility>

extern "C" {

std::uint64_t APS5_VABI WKBundleGetWebNotificationID(const void* bundleRef, const void* notificationRef) {
    NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    const auto* notification = NKWebKit::Cast<NKWebKit::Notification>(notificationRef, __func__);
    return notification->notificationID;
}

void APS5_VABI WKBundleSetWebNotificationPermission(const void* bundleRef, const void* pageRef, const void* originRef, bool allowed) {
    auto* bundle = NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    const auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    const auto* origin = NKWebKit::Cast<NKWebKit::SecurityOrigin>(originRef, __func__);
    bundle->notificationPermissions[std::make_pair(page, origin)] = allowed;
}

}
