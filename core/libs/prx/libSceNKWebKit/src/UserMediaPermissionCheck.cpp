#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

void APS5_VABI WKUserMediaPermissionCheckSetUserMediaAccessInfo(const void* checkRef, const void* userMediaIDRef, bool isAllowed) {
    auto* check = NKWebKit::Cast<NKWebKit::UserMediaPermissionCheck>(checkRef, __func__);
    const auto* userMediaID = NKWebKit::CastOrNull<NKWebKit::String>(userMediaIDRef, __func__);
    check->SetUserMediaAccessInfo(userMediaID ? NKWebKit::Retained<NKWebKit::String>::Share(const_cast<NKWebKit::String*>(userMediaID)) : NKWebKit::Retained<NKWebKit::String>{}, isAllowed);
}

}
