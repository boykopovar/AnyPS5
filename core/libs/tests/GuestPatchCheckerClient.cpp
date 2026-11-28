#include "prx/libScePatchCheckerClient/libScePatchCheckerClient.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

} // namespace

int main() {
    constexpr int notInitialized = static_cast<int>(0x80182402u);
    constexpr int alreadyInitialized = static_cast<int>(0x80182401u);
    for (int call = 0; call < 2; ++call) {
        Require(scePatchCheckerCancel() == notInitialized);
        Require(scePatchCheckerCheckPatch() == notInitialized);
        Require(scePatchCheckerCreateHandler() == notInitialized);
        Require(scePatchCheckerDestroyHandler() == notInitialized);
        Require(scePatchCheckerDisableAutoDownload() == notInitialized);
        Require(scePatchCheckerEnableAutoDownload() == notInitialized);
        Require(scePatchCheckerGetApplicableTick() == notInitialized);
        Require(scePatchCheckerGetPackageInfo() == notInitialized);
        Require(scePatchCheckerRequestCheckPatch() == notInitialized);
        Require(scePatchCheckerRequestCheckPatchByType() == notInitialized);
        Require(scePatchCheckerTerminate() == 0);
    }
    Require(scePatchCheckerInitialize() == 0);
    Require(scePatchCheckerInitialize() == alreadyInitialized);
    Require(scePatchCheckerTerminate() == 0);
    Require(scePatchCheckerTerminate() == 0);
    Require(scePatchCheckerCancel() == notInitialized);
    return 0;
}
