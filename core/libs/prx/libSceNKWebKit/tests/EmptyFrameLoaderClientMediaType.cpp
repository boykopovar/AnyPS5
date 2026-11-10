#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>

using NKWebKit::WtfAbi::String;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "EmptyFrameLoaderClient media type check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

int main() {
    String mediaType = _ZNK7WebCore22EmptyFrameLoaderClient17overrideMediaTypeEv(nullptr);
    Require(mediaType.impl == nullptr);
    return 0;
}
