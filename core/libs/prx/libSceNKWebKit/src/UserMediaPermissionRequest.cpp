#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

void APS5_VABI WKUserMediaPermissionRequestAllow(const void* requestRef) {
    auto* request = NKWebKit::Cast<NKWebKit::UserMediaPermissionRequest>(requestRef, __func__);
    request->Allow();
}

void APS5_VABI WKUserMediaPermissionRequestDeny(const void* requestRef) {
    auto* request = NKWebKit::Cast<NKWebKit::UserMediaPermissionRequest>(requestRef, __func__);
    request->Deny();
}

}
