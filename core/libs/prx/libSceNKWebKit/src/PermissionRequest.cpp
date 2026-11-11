#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

void APS5_VABI WKGeolocationPermissionRequestAllow(const void* permissionRequestRef) {
    auto* request = NKWebKit::Cast<NKWebKit::GeolocationPermissionRequest>(permissionRequestRef, __func__);
    request->Allow();
}

void APS5_VABI WKGeolocationPermissionRequestDeny(const void* permissionRequestRef) {
    auto* request = NKWebKit::Cast<NKWebKit::GeolocationPermissionRequest>(permissionRequestRef, __func__);
    request->Deny();
}

void APS5_VABI WKNotificationPermissionRequestAllow(const void* permissionRequestRef) {
    auto* request = NKWebKit::Cast<NKWebKit::NotificationPermissionRequest>(permissionRequestRef, __func__);
    request->Allow();
}

void APS5_VABI WKNotificationPermissionRequestDeny(const void* permissionRequestRef) {
    auto* request = NKWebKit::Cast<NKWebKit::NotificationPermissionRequest>(permissionRequestRef, __func__);
    request->Deny();
}

}
