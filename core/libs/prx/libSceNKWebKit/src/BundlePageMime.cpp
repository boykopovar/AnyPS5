#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/MimeExports.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

extern "C" {

bool APS5_VABI WKBundlePageCanShowMIMEType(const void* pageRef, const void* mimeTypeRef) {
    NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    const auto* text = NKWebKit::Cast<NKWebKit::String>(mimeTypeRef, __func__);
    if (!text || text->null || text->characters.empty()) {
        return false;
    }
    NKWebKit::WtfAbi::String wtfMime;
    _ZN3WTF6StringC1EPKDsj(&wtfMime, text->characters.data(), static_cast<unsigned>(text->characters.size()));
    return _ZN7WebCore16MIMETypeRegistry15canShowMIMETypeERKN3WTF6StringE(wtfMime);
}

}
