#include "prx/libSceRnpsAppMgr/libSceRnpsAppMgr.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int NotInitialized = static_cast<int>(0x81850001u);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "RNPS APP MGR: %s\n", message);
    std::abort();
}

} // namespace

int main() {
    const std::array<decltype(&sceRnpsAppMgrGetEvent), 25> errorCalls{
        sceRnpsAppMgrBlockAppInstall, sceRnpsAppMgrCreateUpdateRequest, sceRnpsAppMgrCreateUrlRequest,
        sceRnpsAppMgrDestroyBundle, sceRnpsAppMgrDestroyUpdateRequest, sceRnpsAppMgrFinishUrlRequest,
        sceRnpsAppMgrGetAppInfo, sceRnpsAppMgrGetAppInfoDebugString, sceRnpsAppMgrGetAppInfoList,
        sceRnpsAppMgrGetEvent, sceRnpsAppMgrGetMboInfo, sceRnpsAppMgrGetMboManifestDebugString,
        sceRnpsAppMgrGetUpdateProgress, sceRnpsAppMgrRecoverUfsImage, sceRnpsAppMgrRemoveApp,
        sceRnpsAppMgrRemoveAppOnSystemShutdown, sceRnpsAppMgrRemoveUfsImageOnSystemShutdown,
        sceRnpsAppMgrRequestUpdateCheck, sceRnpsAppMgrSetCrlUrl, sceRnpsAppMgrSetEventFlagForClients,
        sceRnpsAppMgrSetUpdateUrl, sceRnpsAppMgrStartService, sceRnpsAppMgrUnblockAppInstall,
        sceRnpsAppMgrVerifyApp, sceRnpsAppMgrVerifyFileInApp};
    const std::array<decltype(&sceRnpsAppMgrIsCrlUpdated), 3> falseCalls{
        sceRnpsAppMgrIsBgsInstallReady, sceRnpsAppMgrIsCrlUpdated, sceRnpsAppMgrIsMboUpdated};
    for (int pass = 0; pass < 2; ++pass) {
        for (const auto call : errorCalls) {
            Require(call() == NotInitialized, "a call must report that the app manager is not initialized");
        }
        for (const auto call : falseCalls) {
            Require(call() == 0, "a query must answer 0 while the app manager is not initialized");
        }
    }
}
