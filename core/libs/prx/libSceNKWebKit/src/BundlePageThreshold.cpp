#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

void APS5_VABI WKBundlePageSetPaintedObjectsCounterThreshold(const void* pageRef, std::uint64_t threshold) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    page->paintedObjectsCounterThreshold = threshold;
}

}
