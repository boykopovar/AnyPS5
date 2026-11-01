#include "prx/libSceNpFriendListDialog/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceIme/include/ModalDialog.hpp"
#include "prx/libSceIme/include/PsnDialogNotice.hpp"
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kStatusNone = 0;
constexpr int kStatusInitialized = 1;
constexpr int kStatusFinished = 3;
constexpr int kErrNotInitialized = static_cast<int>(0x80B80003);
constexpr int kErrAlreadyInitialized = static_cast<int>(0x80B80004);
constexpr int kErrNotFinished = static_cast<int>(0x80B80005);
constexpr int kErrArgNull = static_cast<int>(0x80B8000D);
constexpr int kResultUserCanceled = 1;

bool PsnNoticeShown() {
    ModalDialogView view;
    if (!ModalDialogReadView_nid_postfix(view)) return false;
    return view.message == PsnDialogNoticeRequest(PsnDialogNoticeKind::PsnFeature).message;
}

void RequireNotices() {
    std::uint64_t param = 0;
    ModalDialogSetPresenter_nid_postfix(true);
    Require(sceNpFriendListDialogInitialize() == 0);

    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Silent);
    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    Require(sceNpFriendListDialogOpenA(&param) == 0);
    Require(!PsnNoticeShown() && !ModalDialogCapturesInput_nid_postfix());

    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Show);
    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    Require(sceNpFriendListDialogOpenA(nullptr) == kErrArgNull);
    Require(!PsnNoticeShown());
    Require(sceNpFriendListDialogOpenA(&param) == 0);
    Require(PsnNoticeShown());

    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    ModalDialogSetPresenter_nid_postfix(false);
    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Silent);
    Require(sceNpFriendListDialogTerminate() == 0);
}

}

int main() {
    std::int64_t param = 0;
    std::int32_t result[2] = {-1, -1};

    Require(sceNpFriendListDialogTerminate() == kErrNotInitialized);
    Require(sceNpFriendListDialogOpenA(nullptr) == kErrNotInitialized);
    Require(sceNpFriendListDialogOpenA(&param) == kErrNotInitialized);
    Require(sceNpFriendListDialogClose() == kErrNotInitialized);
    Require(sceNpFriendListDialogGetResultA(nullptr) == kErrNotInitialized);
    Require(sceNpFriendListDialogGetResultA(result) == kErrNotInitialized);
    Require(sceNpFriendListDialogGetStatus() == kStatusNone);
    Require(sceNpFriendListDialogUpdateStatus() == kStatusNone);

    Require(sceNpFriendListDialogInitialize() == 0);
    Require(sceNpFriendListDialogInitialize() == kErrAlreadyInitialized);
    Require(sceNpFriendListDialogGetStatus() == kStatusInitialized);
    Require(sceNpFriendListDialogUpdateStatus() == kStatusInitialized);

    Require(sceNpFriendListDialogOpenA(nullptr) == kErrArgNull);
    Require(sceNpFriendListDialogGetResultA(result) == kErrNotFinished);

    Require(sceNpFriendListDialogOpenA(&param) == 0);
    Require(sceNpFriendListDialogGetStatus() == kStatusFinished);
    Require(sceNpFriendListDialogUpdateStatus() == kStatusFinished);

    Require(sceNpFriendListDialogGetResultA(nullptr) == kErrArgNull);
    Require(sceNpFriendListDialogGetResultA(result) == 0);
    Require(result[0] == kResultUserCanceled);
    Require(result[1] == -1);

    Require(sceNpFriendListDialogClose() == 0);
    Require(sceNpFriendListDialogTerminate() == 0);
    Require(sceNpFriendListDialogTerminate() == kErrNotInitialized);
    Require(sceNpFriendListDialogOpenA(nullptr) == kErrNotInitialized);
    Require(sceNpFriendListDialogOpenA(&param) == kErrNotInitialized);
    Require(sceNpFriendListDialogClose() == kErrNotInitialized);
    Require(sceNpFriendListDialogGetResultA(nullptr) == kErrNotInitialized);
    Require(sceNpFriendListDialogGetResultA(result) == kErrNotInitialized);
    Require(sceNpFriendListDialogGetStatus() == kStatusNone);
    Require(sceNpFriendListDialogUpdateStatus() == kStatusNone);

    Require(sceNpFriendListDialogInitialize() == 0);
    Require(sceNpFriendListDialogUpdateStatus() == kStatusInitialized);
    Require(sceNpFriendListDialogClose() == 0);
    Require(sceNpFriendListDialogUpdateStatus() == kStatusFinished);
    result[0] = -1;
    Require(sceNpFriendListDialogGetResultA(result) == 0);
    Require(result[0] == kResultUserCanceled);
    Require(sceNpFriendListDialogTerminate() == 0);
    Require(sceNpFriendListDialogTerminate() == kErrNotInitialized);
    Require(sceNpFriendListDialogOpenA(nullptr) == kErrNotInitialized);
    Require(sceNpFriendListDialogOpenA(&param) == kErrNotInitialized);
    Require(sceNpFriendListDialogClose() == kErrNotInitialized);
    Require(sceNpFriendListDialogGetResultA(nullptr) == kErrNotInitialized);
    Require(sceNpFriendListDialogGetResultA(result) == kErrNotInitialized);
    Require(sceNpFriendListDialogGetStatus() == kStatusNone);
    Require(sceNpFriendListDialogUpdateStatus() == kStatusNone);
    RequireNotices();
}
