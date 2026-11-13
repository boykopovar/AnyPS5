#include "prx/libSceAppInstUtil/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void RequireThrows(auto function, std::source_location location = std::source_location::current()) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return;
    }
    std::fprintf(stderr, "expected a throw from %s:%d\n", location.file_name(), location.line());
    std::abort();
}

}

int main() {
    RequireThrows([] { sceAppInstUtilAppPrepareOverwritePkg("package:/test/overwrite.pkg"); });
    RequireThrows([] { sceAppInstUtilAppPrepareOverwritePkg(nullptr); });
    RequireThrows([] { sceAppInstUtilAppRecoverApp("PPSA00000"); });
    RequireThrows([] { sceAppInstUtilAppRecoverApp(nullptr); });
    RequireThrows([] { sceAppInstUtilAppUnInstallTheme("PPSA00000-THME00001"); });
    RequireThrows([] { sceAppInstUtilAppUnInstallTheme(nullptr); });
    RequireThrows([] { sceAppInstUtilAppPrepareOverwritePkg(""); });
    RequireThrows([] { sceAppInstUtilAppRecoverApp(""); });
    RequireThrows([] { sceAppInstUtilAppUnInstallTheme(""); });
    return 0;
}
