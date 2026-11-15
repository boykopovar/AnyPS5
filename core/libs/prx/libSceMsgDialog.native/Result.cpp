#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" int APS5_VABI sceMsgDialogGetResult(MsgDialogResult* result) {
    constexpr int statusFinished = 3;
    constexpr int errorNotInitialized = static_cast<int>(0x80B80003u);
    constexpr int errorNotFinished = static_cast<int>(0x80B80005u);
    constexpr int errorArgNull = static_cast<int>(0x80B8000Du);
    int buttonId = 0;
    const int status = MsgDialogResultStatus(buttonId);
    if (status == 0) return errorNotInitialized;
    if (result == nullptr) return errorArgNull;
    if (status != statusFinished) return errorNotFinished;
    MsgDialogResult value{};
    value.button_id = buttonId;
    if (!GuestCopyOut_nid_no_patch(result, &value, sizeof(value))) return errorArgNull;
    return 0;
}
