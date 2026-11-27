#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include <utility>

extern "C" {

void APS5_VABI WKFormSubmissionListenerContinue(const void* listenerRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::FormSubmissionListener>(listenerRef, __func__);
    listener->continued = true;
    if (listener->completionHandler) {
        auto handler = std::exchange(listener->completionHandler, nullptr);
        handler();
    }
}

}
