#include "prx/libScePlayerInvitationDialog/libScePlayerInvitationDialog.h"
#include "prx/libSceCommonDialog/include/Api.hpp"
#include "prx/libSceCommonDialog/include/DialogUse.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void expect_nid_no_patch(std::int32_t actual, std::int32_t expected) {
    if (actual != expected) {
        std::fprintf(stderr, "expected 0x%08x, got 0x%08x\n", static_cast<unsigned>(expected), static_cast<unsigned>(actual));
        std::abort();
    }
}

}

int main() {
    using enum PlayerInvitationDialogStatus;
    constexpr auto notInitialized = static_cast<std::int32_t>(0x80B80003u);
    constexpr auto alreadyInitialized = static_cast<std::int32_t>(0x80B80004u);
    constexpr auto notFinished = static_cast<std::int32_t>(0x80B80005u);
    constexpr auto busy = static_cast<std::int32_t>(0x80B80008u);
    constexpr auto argNull = static_cast<std::int32_t>(0x80B8000Du);
    constexpr std::int32_t userCanceled = 1;
    ScePlayerInvitationDialogResult result{};
    expect_nid_no_patch(scePlayerInvitationDialogGetResult(&result), notInitialized);
    expect_nid_no_patch(scePlayerInvitationDialogUpdateStatus(), static_cast<std::int32_t>(None));
    expect_nid_no_patch(scePlayerInvitationDialogClose(), 0);
    expect_nid_no_patch(scePlayerInvitationDialogTerminate(), notInitialized);
    expect_nid_no_patch(scePlayerInvitationDialogOpen(nullptr), argNull);

    expect_nid_no_patch(sceCommonDialogInitialize(), 0);
    expect_nid_no_patch(CommonDialogAcquire_nid_no_patch(), 1);
    expect_nid_no_patch(scePlayerInvitationDialogInitialize(), busy);
    expect_nid_no_patch(scePlayerInvitationDialogGetStatus(), static_cast<std::int32_t>(None));
    expect_nid_no_patch(sceCommonDialogIsUsed(), 1);
    CommonDialogRelease_nid_no_patch();

    expect_nid_no_patch(scePlayerInvitationDialogInitialize(), 0);
    expect_nid_no_patch(scePlayerInvitationDialogInitialize(), alreadyInitialized);
    expect_nid_no_patch(scePlayerInvitationDialogGetStatus(), static_cast<std::int32_t>(Initialized));
    expect_nid_no_patch(scePlayerInvitationDialogUpdateStatus(), static_cast<std::int32_t>(Initialized));
    expect_nid_no_patch(scePlayerInvitationDialogGetResult(nullptr), argNull);
    expect_nid_no_patch(scePlayerInvitationDialogGetResult(&result), notFinished);

    ScePlayerInvitationDialogParam param{};
    expect_nid_no_patch(scePlayerInvitationDialogOpen(nullptr), argNull);
    expect_nid_no_patch(scePlayerInvitationDialogGetStatus(), static_cast<std::int32_t>(Initialized));
    expect_nid_no_patch(scePlayerInvitationDialogOpen(&param), 0);
    expect_nid_no_patch(scePlayerInvitationDialogGetStatus(), static_cast<std::int32_t>(Running));
    expect_nid_no_patch(scePlayerInvitationDialogGetResult(&result), notFinished);
    expect_nid_no_patch(scePlayerInvitationDialogUpdateStatus(), static_cast<std::int32_t>(Finished));
    std::memset(&result, 0xFF, sizeof(result));
    expect_nid_no_patch(scePlayerInvitationDialogGetResult(&result), 0);
    expect_nid_no_patch(result.errorCode, 0);
    expect_nid_no_patch(result.result, userCanceled);
    for (const std::uint8_t byte : result.reserved) expect_nid_no_patch(byte, 0);
    expect_nid_no_patch(scePlayerInvitationDialogUpdateStatus(), static_cast<std::int32_t>(Finished));
    expect_nid_no_patch(scePlayerInvitationDialogOpen(nullptr), argNull);
    expect_nid_no_patch(scePlayerInvitationDialogClose(), 0);
    expect_nid_no_patch(scePlayerInvitationDialogGetStatus(), static_cast<std::int32_t>(Finished));
    expect_nid_no_patch(scePlayerInvitationDialogTerminate(), 0);
    expect_nid_no_patch(scePlayerInvitationDialogTerminate(), notInitialized);
    expect_nid_no_patch(scePlayerInvitationDialogGetStatus(), static_cast<std::int32_t>(None));
    expect_nid_no_patch(scePlayerInvitationDialogGetResult(&result), notInitialized);

    expect_nid_no_patch(scePlayerInvitationDialogOpen(&param), notInitialized);
    expect_nid_no_patch(scePlayerInvitationDialogInitialize(), 0);
    expect_nid_no_patch(scePlayerInvitationDialogOpen(&param), 0);
    expect_nid_no_patch(scePlayerInvitationDialogGetStatus(), static_cast<std::int32_t>(Running));
    expect_nid_no_patch(scePlayerInvitationDialogUpdateStatus(), static_cast<std::int32_t>(Finished));
    expect_nid_no_patch(scePlayerInvitationDialogTerminate(), 0);
}
