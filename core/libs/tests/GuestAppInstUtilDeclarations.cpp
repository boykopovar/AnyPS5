#include "prx/libSceAppInstUtil/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Testing.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

}

int main() {
    ExpectErr<std::runtime_error>([] { sceAppInstUtilAppPrepareOverwritePkg("package:/test/overwrite.pkg"); });
    ExpectErr<std::runtime_error>([] { sceAppInstUtilAppPrepareOverwritePkg(nullptr); });
    ExpectErr<std::runtime_error>([] { sceAppInstUtilAppRecoverApp("PPSA00000"); });
    ExpectErr<std::runtime_error>([] { sceAppInstUtilAppRecoverApp(nullptr); });
    ExpectErr<std::runtime_error>([] { sceAppInstUtilAppUnInstallTheme("PPSA00000-THME00001"); });
    ExpectErr<std::runtime_error>([] { sceAppInstUtilAppUnInstallTheme(nullptr); });
    return 0;
}
