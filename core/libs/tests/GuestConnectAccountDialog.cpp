#include "prx/libSceConnectAccountDialog/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceIme/include/ModalDialog.hpp"
#include "prx/libSceIme/include/PsnDialogNotice.hpp"
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <thread>

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
constexpr int kRaceRounds = 200000;

template <typename Finish>
void RequireTerminateWins(Finish finish) {
    std::atomic<int> round{0};
    std::atomic<int> done{0};
    std::thread terminator([&] {
        for (int i = 1; i <= kRaceRounds; ++i) {
            while (round.load() != i) std::this_thread::yield();
            sceConnectAccountDialogTerminate();
            done.fetch_add(1);
        }
    });
    for (int i = 1; i <= kRaceRounds; ++i) {
        Require(sceConnectAccountDialogInitialize() == 0);
        round.store(i);
        finish();
        while (done.load() != i) std::this_thread::yield();
        Require(sceConnectAccountDialogGetStatus() == kStatusNone);
    }
    terminator.join();
}

bool PsnNoticeShown() {
    ModalDialogView view;
    if (!ModalDialogReadView_nid_postfix(view)) return false;
    return view.message == PsnDialogNoticeRequest(PsnDialogNoticeKind::PsnFeature).message;
}

void RequireNotices() {
    std::uint64_t param = 0;
    ModalDialogSetPresenter_nid_postfix(true);
    Require(sceConnectAccountDialogInitialize() == 0);

    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Silent);
    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    Require(sceConnectAccountDialogOpen(&param) == 0);
    Require(!PsnNoticeShown() && !ModalDialogCapturesInput_nid_postfix());

    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Show);
    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    Require(sceConnectAccountDialogOpen(nullptr) == kErrArgNull);
    Require(!PsnNoticeShown());
    Require(sceConnectAccountDialogOpen(&param) == 0);
    Require(PsnNoticeShown());

    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    ModalDialogSetPresenter_nid_postfix(false);
    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Silent);
    Require(sceConnectAccountDialogTerminate() == 0);
}

}

int main() {
    std::int64_t param = 0;
    std::int32_t result[2] = {-1, -1};

    Require(sceConnectAccountDialogTerminate() == kErrNotInitialized);
    Require(sceConnectAccountDialogOpen(nullptr) == kErrNotInitialized);
    Require(sceConnectAccountDialogOpen(&param) == kErrNotInitialized);
    Require(sceConnectAccountDialogClose() == kErrNotInitialized);
    Require(sceConnectAccountDialogGetResult(nullptr) == kErrNotInitialized);
    Require(sceConnectAccountDialogGetResult(result) == kErrNotInitialized);
    Require(sceConnectAccountDialogGetStatus() == kStatusNone);
    Require(sceConnectAccountDialogUpdateStatus() == kStatusNone);

    Require(sceConnectAccountDialogInitialize() == 0);
    Require(sceConnectAccountDialogInitialize() == kErrAlreadyInitialized);
    Require(sceConnectAccountDialogGetStatus() == kStatusInitialized);
    Require(sceConnectAccountDialogUpdateStatus() == kStatusInitialized);

    Require(sceConnectAccountDialogOpen(nullptr) == kErrArgNull);
    Require(sceConnectAccountDialogGetResult(result) == kErrNotFinished);

    Require(sceConnectAccountDialogOpen(&param) == 0);
    Require(sceConnectAccountDialogGetStatus() == kStatusFinished);
    Require(sceConnectAccountDialogUpdateStatus() == kStatusFinished);

    Require(sceConnectAccountDialogGetResult(nullptr) == kErrArgNull);
    errno = 71;
    Require(sceConnectAccountDialogGetResult(reinterpret_cast<void*>(0x10)) == kErrArgNull);
    Require(errno == 71);
    Require(sceConnectAccountDialogGetResult(result) == 0);
    Require(result[0] == kResultUserCanceled);
    Require(result[1] == -1);

    Require(sceConnectAccountDialogClose() == 0);
    Require(sceConnectAccountDialogTerminate() == 0);
    Require(sceConnectAccountDialogTerminate() == kErrNotInitialized);
    Require(sceConnectAccountDialogOpen(nullptr) == kErrNotInitialized);
    Require(sceConnectAccountDialogOpen(&param) == kErrNotInitialized);
    Require(sceConnectAccountDialogClose() == kErrNotInitialized);
    Require(sceConnectAccountDialogGetResult(nullptr) == kErrNotInitialized);
    Require(sceConnectAccountDialogGetResult(result) == kErrNotInitialized);
    Require(sceConnectAccountDialogGetStatus() == kStatusNone);
    Require(sceConnectAccountDialogUpdateStatus() == kStatusNone);

    Require(sceConnectAccountDialogInitialize() == 0);
    Require(sceConnectAccountDialogUpdateStatus() == kStatusInitialized);
    Require(sceConnectAccountDialogClose() == 0);
    Require(sceConnectAccountDialogUpdateStatus() == kStatusFinished);
    result[0] = -1;
    Require(sceConnectAccountDialogGetResult(result) == 0);
    Require(result[0] == kResultUserCanceled);
    errno = 71;
    Require(sceConnectAccountDialogGetResult(reinterpret_cast<void*>(0x10)) == kErrArgNull);
    Require(errno == 71);
    Require(sceConnectAccountDialogTerminate() == 0);
    Require(sceConnectAccountDialogTerminate() == kErrNotInitialized);
    Require(sceConnectAccountDialogOpen(nullptr) == kErrNotInitialized);
    Require(sceConnectAccountDialogOpen(&param) == kErrNotInitialized);
    Require(sceConnectAccountDialogClose() == kErrNotInitialized);
    Require(sceConnectAccountDialogGetResult(nullptr) == kErrNotInitialized);
    Require(sceConnectAccountDialogGetResult(result) == kErrNotInitialized);
    Require(sceConnectAccountDialogGetStatus() == kStatusNone);
    Require(sceConnectAccountDialogUpdateStatus() == kStatusNone);
    RequireNotices();
    RequireTerminateWins([&] { sceConnectAccountDialogOpen(&param); });
    RequireTerminateWins([] { sceConnectAccountDialogClose(); });
}
