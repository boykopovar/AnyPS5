#include "prx/libScePlayerInvitationDialog/libScePlayerInvitationDialog.h"

#include <cstdlib>
#include <cstring>

namespace {

void expect(std::int32_t actual, std::int32_t expected) {
    if (actual != expected) {
        std::abort();
    }
}

}

int main() {
    using enum PlayerInvitationDialogStatus;
    constexpr auto notInitialized = static_cast<std::int32_t>(0x80B80003u);
    constexpr auto alreadyInitialized = static_cast<std::int32_t>(0x80B80004u);
    constexpr auto notFinished = static_cast<std::int32_t>(0x80B80005u);
    constexpr auto argNull = static_cast<std::int32_t>(0x80B8000Du);
    constexpr std::int32_t userCanceled = 1;
    ScePlayerInvitationDialogResult result{};
    expect(scePlayerInvitationDialogGetResult(&result), notInitialized);
    expect(scePlayerInvitationDialogUpdateStatus(), static_cast<std::int32_t>(None));
    expect(scePlayerInvitationDialogClose(), 0);
    expect(scePlayerInvitationDialogTerminate(), notInitialized);
    expect(scePlayerInvitationDialogOpen(nullptr), argNull);

    expect(scePlayerInvitationDialogInitialize(), 0);
    expect(scePlayerInvitationDialogInitialize(), alreadyInitialized);
    expect(scePlayerInvitationDialogGetStatus(), static_cast<std::int32_t>(Initialized));
    expect(scePlayerInvitationDialogUpdateStatus(), static_cast<std::int32_t>(Initialized));
    expect(scePlayerInvitationDialogGetResult(nullptr), argNull);
    expect(scePlayerInvitationDialogGetResult(&result), notFinished);

    ScePlayerInvitationDialogParam param{};
    expect(scePlayerInvitationDialogOpen(nullptr), argNull);
    expect(scePlayerInvitationDialogGetStatus(), static_cast<std::int32_t>(Initialized));
    expect(scePlayerInvitationDialogOpen(&param), 0);
    expect(scePlayerInvitationDialogGetStatus(), static_cast<std::int32_t>(Running));
    expect(scePlayerInvitationDialogGetResult(&result), notFinished);
    expect(scePlayerInvitationDialogUpdateStatus(), static_cast<std::int32_t>(Finished));
    std::memset(&result, 0xFF, sizeof(result));
    expect(scePlayerInvitationDialogGetResult(&result), 0);
    expect(result.errorCode, 0);
    expect(result.result, userCanceled);
    for (const std::uint8_t byte : result.reserved) expect(byte, 0);
    expect(scePlayerInvitationDialogUpdateStatus(), static_cast<std::int32_t>(Finished));
    expect(scePlayerInvitationDialogOpen(nullptr), argNull);
    expect(scePlayerInvitationDialogClose(), 0);
    expect(scePlayerInvitationDialogGetStatus(), static_cast<std::int32_t>(Finished));
    expect(scePlayerInvitationDialogTerminate(), 0);
    expect(scePlayerInvitationDialogTerminate(), notInitialized);
    expect(scePlayerInvitationDialogGetStatus(), static_cast<std::int32_t>(None));
    expect(scePlayerInvitationDialogGetResult(&result), notInitialized);

    expect(scePlayerInvitationDialogOpen(&param), notInitialized);
    expect(scePlayerInvitationDialogInitialize(), 0);
    expect(scePlayerInvitationDialogOpen(&param), 0);
    expect(scePlayerInvitationDialogGetStatus(), static_cast<std::int32_t>(Running));
    expect(scePlayerInvitationDialogUpdateStatus(), static_cast<std::int32_t>(Finished));
    expect(scePlayerInvitationDialogTerminate(), 0);
}
