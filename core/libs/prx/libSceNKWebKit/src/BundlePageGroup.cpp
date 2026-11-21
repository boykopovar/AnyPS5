#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

const void* APS5_VABI WKBundlePageGroupCopyIdentifier(const void* pageGroupRef) {
    const auto* pageGroup = NKWebKit::Cast<NKWebKit::BundlePageGroup>(pageGroupRef, __func__);
    return NKWebKit::ToRef(pageGroup->identifier ? pageGroup->identifier : NKWebKit::String::FromUtf8(""));
}

}
