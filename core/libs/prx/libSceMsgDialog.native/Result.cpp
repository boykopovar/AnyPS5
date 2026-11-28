#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>

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
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(result, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) return errorArgNull;
    return 0;
}
