#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

const void* APS5_VABI WKSessionStateCreateFromData(const void* data) {
    if (!data) return nullptr;
    const auto* d = NKWebKit::Cast<NKWebKit::Data>(data, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::SessionState>(d->bytes));
}

const void* APS5_VABI WKSessionStateCopyData(const void* sessionState) {
    if (!sessionState) return nullptr;
    const auto* state = NKWebKit::Cast<NKWebKit::SessionState>(sessionState, __func__);
    return WKDataCreate(state->data.data(), state->data.size());
}

}
