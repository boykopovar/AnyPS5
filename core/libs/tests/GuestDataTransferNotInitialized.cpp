#include "prx/libSceDataTransfer/libSceDataTransfer.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int NotInitialized = static_cast<int>(0x81620000);

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

using Export = decltype(&sceDataTransferAbortRequest);

const std::array<Export, 40> GatedExports = {
    sceDataTransferAbortFgTransfer,
    sceDataTransferAbortRequest,
    sceDataTransferAbortSearchPS4,
    sceDataTransferCheckBgTransferRunning,
    sceDataTransferGetFgTransferProgress,
    sceDataTransferGetPrepareFgTransferProgress,
    sceDataTransferRequestGetAppInfoPS4,
    sceDataTransferRequestGetSavedataInfoPS4,
    sceDataTransferRequestGetUsersPS4,
    sceDataTransferRequestLaunchPS4,
    sceDataTransferRequestPrepareFgTransfer,
    sceDataTransferRequestRebootAuthPS4,
    sceDataTransferRequestSearchPS4,
    sceDataTransferRequestStartFgTransfer,
    sceDataTransferRequestTransferTimePS4,
    sceDataTransferAbortAuthPS5,
    sceDataTransferAbortFgTransferPS5,
    sceDataTransferAbortLaunchPS4,
    sceDataTransferAbortLaunchPS5,
    sceDataTransferAbortPS5Request,
    sceDataTransferAbortRequestBeforeFgTransferPS5,
    sceDataTransferAbortSearchPS5,
    sceDataTransferGetFgTransferProgressPS5,
    sceDataTransferGetPS4RebootData,
    sceDataTransferHostNotifyEasySignInReady,
    sceDataTransferRequestAuthPS4,
    sceDataTransferRequestAuthPS5,
    sceDataTransferRequestCalcTransferSizePS4,
    sceDataTransferRequestCalcTransferSizePS5,
    sceDataTransferRequestEndFgTransfer,
    sceDataTransferRequestEndFgTransferPS5,
    sceDataTransferRequestGetDeviceInfoPS4,
    sceDataTransferRequestGetPS4Info,
    sceDataTransferRequestGetPS5Info,
    sceDataTransferRequestLaunchPS5,
    sceDataTransferRequestPrepareFgTransferPS5,
    sceDataTransferRequestRebootAuthPS5,
    sceDataTransferRequestSearchPS5,
    sceDataTransferRequestStartFgTransferPS5,
    sceDataTransferRequestTransferTimePS5
};

} // namespace

int main() {
    for (std::size_t i = 0; i < GatedExports.size(); ++i) {
        if (GatedExports[i]() == NotInitialized) continue;
        std::fprintf(stderr, "export %zu\n", i);
        Require(false, "an export answers 0x81620000 before initialization");
    }
    return 0;
}
