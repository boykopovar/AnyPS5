#include "prx/libSceFlexContent/libSceFlexContent.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

} // namespace

int main() {
    constexpr int notInitialized = static_cast<int>(0x81B60001u);
    constexpr int alreadyInitialized = static_cast<int>(0x81B60002u);
    for (int call = 0; call < 2; ++call) {
        Require(sceFlexContentTerminate() == notInitialized);
        Require(sceFlexContentUnknown00() == notInitialized);
        Require(sceFlexContentUnknown01() == notInitialized);
        Require(sceFlexContentMount() == notInitialized);
        Require(sceFlexContentUnknown02() == notInitialized);
        Require(sceFlexContentGetDownloadProgress() == notInitialized);
        Require(sceFlexContentDelete() == notInitialized);
        Require(sceFlexContentGetLatestVersion() == notInitialized);
        Require(sceFlexContentGetInstalledVersion() == notInitialized);
        Require(sceFlexContentUnknown03() == notInitialized);
        Require(sceFlexContentUnknown04() == notInitialized);
    }
    Require(sceFlexContentInitialize() == 0);
    Require(sceFlexContentInitialize() == alreadyInitialized);
    Require(sceFlexContentTerminate() == 0);
    Require(sceFlexContentTerminate() == notInitialized);
    return 0;
}
