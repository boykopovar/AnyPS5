#include "prx/libSceNKWebKit/include/InventoryExports.hpp"

#include <cstdio>
#include <cstdlib>

int main() {
    _ZN7WebCore22EmptyFrameLoaderClient25setMainFrameDocumentReadyEb(false);
    _ZN7WebCore22EmptyFrameLoaderClient25setMainFrameDocumentReadyEb(true);
    std::puts("EmptyFrameLoaderClient document-ready tests passed");
    return 0;
}
