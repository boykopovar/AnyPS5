#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

void APS5_VABI WKConnectionPostMessage(const void* connectionRef, const void* messageNameRef, const void* messageBodyRef) {
    auto* conn = NKWebKit::Cast<NKWebKit::Connection>(connectionRef, __func__);
    if (messageNameRef) {
        conn->lastMessageName = NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::Object*>(NKWebKit::ToObject(messageNameRef, __func__)));
    }
    if (messageBodyRef) {
        conn->lastMessageBody = NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::Object*>(NKWebKit::ToObject(messageBodyRef, __func__)));
    }
}

void APS5_VABI WKConnectionSetConnectionClient(const void* connectionRef, const void* client) {
    auto* conn = NKWebKit::Cast<NKWebKit::Connection>(connectionRef, __func__);
    conn->clientConfigured = (client != nullptr);
}

}
