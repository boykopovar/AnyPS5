#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

void APS5_VABI WKMessageListenerSendReply(const void* listenerRef, const void* replyRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::MessageListener>(listenerRef, __func__);
    listener->SendReply(replyRef ? NKWebKit::Retained<NKWebKit::Object>::Share(const_cast<NKWebKit::Object*>(NKWebKit::ToObject(replyRef, __func__))) : NKWebKit::Retained<NKWebKit::Object>{});
}

}
