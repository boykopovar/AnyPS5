#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKContextSetPrewarmsProcessesAutomatically(const void* contextRef, bool value) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->prewarmsProcessesAutomatically = value;
}

}
