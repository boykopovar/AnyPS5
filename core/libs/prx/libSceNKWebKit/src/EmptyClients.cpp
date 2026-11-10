#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/Api.hpp"
#include "prx/libSceNKWebKit/include/InventoryExports.hpp"

extern "C" {

void APS5_VABI _ZN7WebCore22EmptyFrameLoaderClient35dispatchDidChangeLocationWithinPageEv() {
}

void APS5_VABI _ZN7WebCore22EmptyFrameLoaderClient35dispatchDidClearWindowObjectInWorldERNS_15DOMWrapperWorldE(WebCore::DOMWrapperWorld&) {
}

NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore22EmptyFrameLoaderClient9userAgentERKN3WTF3URLE(const WebCore::EmptyFrameLoaderClient*, const NKWebKit::WtfAbi::URL&) {
    return NKWebKit::WtfAbi::String(NKWebKit::WtfAbi::EmptyStringImpl());
}

}
