#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

const void* APS5_VABI WKSessionStateCreateFromData(const void* dataRef) {
    const auto* data = NKWebKit::Cast<NKWebKit::Data>(dataRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::SessionState>(data->bytes));
}

const void* APS5_VABI WKSessionStateCopyData(const void* sessionStateRef) {
    const auto* state = NKWebKit::Cast<NKWebKit::SessionState>(sessionStateRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Data>(state->data));
}

}
