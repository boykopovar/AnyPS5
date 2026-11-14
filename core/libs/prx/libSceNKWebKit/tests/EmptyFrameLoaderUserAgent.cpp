#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::URL;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "EmptyFrameLoaderClient user agent check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

int main() {
    const URL url{};
    String userAgent = _ZNK7WebCore22EmptyFrameLoaderClient9userAgentERKN3WTF3URLE(nullptr, url);
    Require(userAgent.impl != nullptr);
    Require(userAgent.impl->length == 0);
    return 0;
}
