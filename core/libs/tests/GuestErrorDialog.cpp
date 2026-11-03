#include "prx/libSceErrorDialog/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceIme/include/ModalDialog.hpp"
#include <cstdint>
#include <chrono>
#include <cstdlib>
#include <thread>

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kStatusRunning = 2;
constexpr int kStatusFinished = 3;
constexpr int kErrNotInitialized = static_cast<int>(0x80ED0001);
constexpr int kErrAlreadyInitialized = static_cast<int>(0x80ED0002);
constexpr int kErrParam = static_cast<int>(0x80ED0003);
constexpr int kErrUnexpectedFatal = static_cast<int>(0x80ED0004);
constexpr int kErrInvalidState = static_cast<int>(0x80ED0005);

struct ErrorDialogParam {
    std::int32_t size;
    std::int32_t error_code;
    std::int32_t user_id;
    std::int32_t reserved;
};

ModalDialogState Session() {
    std::size_t chosen = 0;
    return ModalDialogPoll_nid_postfix(chosen);
}

void CloseAndFallback() {
    ErrorDialogParam param{16, static_cast<std::int32_t>(0x80020001), 0, 0};
    ModalDialogSetPresenter_nid_postfix(true);
    Require(sceErrorDialogInitialize() == 0);
    Require(sceErrorDialogOpen(&param) == 0 && Session() == ModalDialogState::Showing);
    Require(sceErrorDialogClose() == 0);
    Require(Session() == ModalDialogState::Closed);
    Require(sceErrorDialogGetStatus() == kStatusFinished);
    Require(sceErrorDialogClose() == kErrInvalidState);

    Require(sceErrorDialogOpen(&param) == 0 && Session() == ModalDialogState::Showing);
    Require(sceErrorDialogTerminate() == 0);
    Require(Session() == ModalDialogState::Idle);
    Require(!ModalDialogCapturesInput_nid_postfix());

    ModalDialogRequest other;
    other.message = u"Other";
    other.buttons = {u"OK"};
    Require(ModalDialogOpen_nid_postfix(other));
    Require(sceErrorDialogInitialize() == 0);
    Require(sceErrorDialogOpen(&param) == 0);
    Require(sceErrorDialogGetStatus() == kStatusRunning);
    Require(sceErrorDialogUpdateStatus() == kStatusFinished);
    ModalDialogReset_nid_postfix();
    Require(sceErrorDialogTerminate() == 0);

    ModalDialogSetPresenter_nid_postfix(false);
    Require(sceErrorDialogInitialize() == 0);
    Require(sceErrorDialogOpen(&param) == 0 && Session() == ModalDialogState::Idle);
    Require(sceErrorDialogUpdateStatus() == kStatusFinished);
    Require(sceErrorDialogTerminate() == 0);
}

void OtherLibraryDialog() {
    ModalDialogSetPresenter_nid_postfix(true);
    ModalDialogRequest other;
    other.owner = ModalDialogOwnerMsgDialog;
    other.message = u"Question";
    other.buttons = {u"Yes", u"No"};
    Require(ModalDialogOpen_nid_postfix(other));
    Require(sceErrorDialogInitialize() == 0);
    Require(sceErrorDialogTerminate() == 0);
    std::size_t chosen = 0;
    Require(ModalDialogPoll_nid_postfix(chosen, ModalDialogOwnerMsgDialog) == ModalDialogState::Showing && ModalDialogCapturesInput_nid_postfix());
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Confirm);
    Require(ModalDialogPoll_nid_postfix(chosen, ModalDialogOwnerMsgDialog) == ModalDialogState::Answered && chosen == 0);
    ModalDialogReset_nid_postfix();
    ModalDialogSetPresenter_nid_postfix(false);
}

void WithoutFlips() {
    ErrorDialogParam param{16, static_cast<std::int32_t>(0x80020001), 0, 0};
    ModalDialogSetPresenter_nid_postfix(true);
    ModalDialogSetShowTimeout_nid_postfix(40);
    Require(sceErrorDialogInitialize() == 0);
    Require(sceErrorDialogOpen(&param) == 0);
    Require(sceErrorDialogGetStatus() == kStatusRunning);
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    Require(sceErrorDialogUpdateStatus() == kStatusFinished);
    Require(!ModalDialogCapturesInput_nid_postfix() && Session() == ModalDialogState::Unshown);

    Require(sceErrorDialogOpen(&param) == 0 && sceErrorDialogGetStatus() == kStatusRunning);
    ModalDialogView view;
    Require(ModalDialogReadView_nid_postfix(view));
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    Require(sceErrorDialogUpdateStatus() == kStatusRunning);
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Confirm);
    Require(sceErrorDialogUpdateStatus() == kStatusFinished);
    Require(sceErrorDialogTerminate() == 0);
    ModalDialogSetShowTimeout_nid_postfix(ModalDialogShowTimeoutMilliseconds);
    ModalDialogSetPresenter_nid_postfix(false);
}

void ShownDialog() {
    ErrorDialogParam param{16, static_cast<std::int32_t>(0x80020001), 0, 0};
    ModalDialogSetPresenter_nid_postfix(true);
    Require(sceErrorDialogInitialize() == 0);
    Require(sceErrorDialogOpen(&param) == 0);
    Require(Session() == ModalDialogState::Showing);
    ModalDialogView view;
    Require(ModalDialogReadView_nid_postfix(view));
    Require(view.message == u"An error occurred.\nError code: 0x80020001" && view.buttons.size() == 1 && view.buttons[0].selected);
    for (int poll = 0; poll < 3; ++poll) Require(sceErrorDialogUpdateStatus() == kStatusRunning && sceErrorDialogGetStatus() == kStatusRunning);
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Confirm);
    Require(sceErrorDialogGetStatus() == kStatusFinished);
    Require(sceErrorDialogUpdateStatus() == kStatusFinished);

    Require(sceErrorDialogOpen(&param) == 0 && Session() == ModalDialogState::Showing);
    ModalDialogPublishInput_nid_postfix(ModalDialogInputKind::Cancel);
    Require(sceErrorDialogUpdateStatus() == kStatusFinished);
    Require(sceErrorDialogTerminate() == 0);
    ModalDialogSetPresenter_nid_postfix(false);
}

}

int main() {
    ErrorDialogParam param{16, static_cast<std::int32_t>(0x80020001), 0, 0};

    Require(sceErrorDialogGetStatus() == 0);
    Require(sceErrorDialogUpdateStatus() == 0);
    Require(sceErrorDialogTerminate() == kErrNotInitialized);
    Require(sceErrorDialogClose() == kErrInvalidState);
    Require(sceErrorDialogOpen(nullptr) == kErrInvalidState);
    Require(sceErrorDialogOpen(&param) == kErrInvalidState);

    Require(sceErrorDialogInitialize() == 0);
    Require(sceErrorDialogInitialize() == kErrAlreadyInitialized);
    Require(sceErrorDialogGetStatus() == 1);
    Require(sceErrorDialogUpdateStatus() == 1);

    Require(sceErrorDialogClose() == kErrInvalidState);

    Require(sceErrorDialogOpen(nullptr) == kErrParam);
    ErrorDialogParam bad{8, 0, 0, 0};
    Require(sceErrorDialogOpen(&bad) == kErrParam);
    ErrorDialogParam badZero{0, 0, 0, 0};
    Require(sceErrorDialogOpen(&badZero) == kErrParam);
    ErrorDialogParam badLarge{32, 0, 0, 0};
    Require(sceErrorDialogOpen(&badLarge) == kErrParam);

    Require(sceErrorDialogOpen(&param) == 0);
    Require(sceErrorDialogGetStatus() == kStatusRunning);
    Require(sceErrorDialogOpen(nullptr) == kErrInvalidState);
    Require(sceErrorDialogOpen(&param) == kErrInvalidState);

    Require(sceErrorDialogUpdateStatus() == kStatusFinished);
    Require(sceErrorDialogGetStatus() == kStatusFinished);

    Require(sceErrorDialogOpen(&param) == 0);
    Require(sceErrorDialogGetStatus() == kStatusRunning);
    Require(sceErrorDialogClose() == 0);
    Require(sceErrorDialogGetStatus() == kStatusFinished);

    Require(sceErrorDialogTerminate() == 0);
    Require(sceErrorDialogTerminate() == kErrNotInitialized);
    Require(sceErrorDialogGetStatus() == 0);
    Require(sceErrorDialogUpdateStatus() == 0);
    Require(sceErrorDialogClose() == kErrInvalidState);
    Require(sceErrorDialogOpen(nullptr) == kErrInvalidState);
    Require(sceErrorDialogOpen(&param) == kErrInvalidState);
    Require(sceErrorDialogOpenDetail() == kErrUnexpectedFatal);
    Require(sceErrorDialogOpenWithReport() == kErrUnexpectedFatal);

    ShownDialog();
    CloseAndFallback();
    WithoutFlips();
    OtherLibraryDialog();
}
