#include "prx/libSceInvitationDialog/include/Api.hpp"
#include "prx/libSceIme/include/ModalDialog.hpp"
#include "prx/libSceIme/include/PsnDialogNotice.hpp"
#include "prx/libSceCommonDialog/include/Api.hpp"
#include "prx/libSceCommonDialog/include/DialogUse.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr int StatusNone = 0;
constexpr int StatusInitialized = 1;
constexpr int StatusFinished = 3;
constexpr int NotSystemInitialized = static_cast<int>(0x80B80001u);
constexpr int NotInitialized = static_cast<int>(0x80B80003u);
constexpr int AlreadyInitialized = static_cast<int>(0x80B80004u);
constexpr int NotFinished = static_cast<int>(0x80B80005u);
constexpr int Busy = static_cast<int>(0x80B80008u);
constexpr int ArgNull = static_cast<int>(0x80B8000Du);
constexpr std::int32_t ResultUserCanceled = 1;
constexpr std::int32_t ModeSend = 1;
constexpr std::int32_t ModeRecv = 2;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "InvitationDialog: %s\n", message);
        std::abort();
    }
}

template <typename TCall>
void RequireUnsettled(TCall call, const char* message) {
    bool threw = false;
    try {
        call();
    } catch (const std::runtime_error& error) {
        threw = std::string(error.what()).find("not implemented") == std::string::npos;
    }
    Require(threw, message);
}

InvitationDialogParam MakeParam(std::int32_t mode, void* callbackArg) {
    InvitationDialogParam param{};
    param.baseParam.size = sizeof(CommonDialogBaseParam);
    param.size = sizeof(InvitationDialogParam);
    param.mode = mode;
    param.userId = 0x10000000;
    param.callbackArg = callbackArg;
    return param;
}

InvitationDialogParamA MakeParamA(std::int32_t mode, void* callbackArg) {
    InvitationDialogParamA param{};
    param.baseParam.size = sizeof(CommonDialogBaseParam);
    param.size = sizeof(InvitationDialogParamA);
    param.mode = mode;
    param.userId = 0x10000000;
    param.callbackArg = callbackArg;
    return param;
}

void TestBeforeInitialize() {
    InvitationDialogResult result{};
    Require(sceInvitationDialogUpdateStatus() == StatusNone, "status before initialization");
    Require(sceInvitationDialogGetResult(&result) == NotInitialized, "result before initialization");
    Require(sceInvitationDialogGetResult(nullptr) == NotInitialized, "null result before initialization");
    Require(sceInvitationDialogClose() == NotInitialized, "close before initialization");
    Require(sceInvitationDialogTerminate() == NotInitialized, "terminate before initialization");
    const InvitationDialogParam param = MakeParam(ModeSend, nullptr);
    RequireUnsettled([&] { sceInvitationDialogOpen(&param); }, "open before initialization");
    RequireUnsettled([] { sceInvitationDialogOpen(nullptr); }, "null open before initialization");
    errno = 71;
    RequireUnsettled([] { sceInvitationDialogOpen(reinterpret_cast<const InvitationDialogParam*>(0x10)); }, "unmapped open before initialization");
    Require(errno == 71, "errno preserved before initialization");
}

void TestCommonDialogPreconditions() {
    Require(sceInvitationDialogInitialize() == NotSystemInitialized, "initialize before sceCommonDialogInitialize");
    Require(sceInvitationDialogUpdateStatus() == StatusNone && !sceCommonDialogIsUsed(), "a failed initialization leaves no state");
    Require(sceCommonDialogInitialize() == 0, "initialize the common dialog system");

    Require(CommonDialogAcquire_nid_no_patch(), "another common dialog takes the system");
    Require(sceInvitationDialogInitialize() == Busy, "initialize while another common dialog is in use");
    Require(sceInvitationDialogUpdateStatus() == StatusNone, "a busy initialization leaves the status");
    CommonDialogRelease_nid_no_patch();

    Require(sceInvitationDialogInitialize() == 0 && sceCommonDialogIsUsed(), "the dialog takes the system");
    Require(!CommonDialogAcquire_nid_no_patch(), "another common dialog is busy while this one is initialized");
    Require(sceInvitationDialogTerminate() == 0 && !sceCommonDialogIsUsed(), "termination frees the system");
}

void TestOpenChecks() {
    InvitationDialogResult result{};
    Require(sceInvitationDialogInitialize() == 0, "initialize");
    Require(sceInvitationDialogInitialize() == AlreadyInitialized, "initialize twice");
    Require(sceCommonDialogIsUsed(), "the system stays in use after a repeated initialization");
    Require(sceInvitationDialogUpdateStatus() == StatusInitialized, "status after initialization");
    Require(sceInvitationDialogGetResult(&result) == NotFinished, "result before the dialog opened");
    Require(sceInvitationDialogGetResult(nullptr) == NotFinished, "null result before the dialog finished");
    RequireUnsettled([] { sceInvitationDialogClose(); }, "close of a dialog that is not running");

    Require(sceInvitationDialogOpen(nullptr) == ArgNull, "open without a param");
    errno = 71;
    Require(sceInvitationDialogOpen(reinterpret_cast<const InvitationDialogParam*>(0x10)) == ArgNull, "open with unmapped memory");
    Require(errno == 71, "errno preserved when rejecting unmapped memory");
    InvitationDialogParam param = MakeParam(ModeSend, nullptr);
    param.size = sizeof(InvitationDialogParam) - 8;
    RequireUnsettled([&] { sceInvitationDialogOpen(&param); }, "open with another param size");
    param = MakeParam(0, nullptr);
    RequireUnsettled([&] { sceInvitationDialogOpen(&param); }, "open in the invalid mode");
    param = MakeParam(3, nullptr);
    RequireUnsettled([&] { sceInvitationDialogOpen(&param); }, "open in an unknown mode");
    Require(sceInvitationDialogUpdateStatus() == StatusInitialized, "a rejected open leaves the status");
}

void TestCanceled() {
    int first = 0;
    int second = 0;
    const InvitationDialogParam send = MakeParam(ModeSend, &first);
    Require(sceInvitationDialogOpen(&send) == 0, "open to send");
    Require(sceInvitationDialogUpdateStatus() == StatusFinished, "the dialog finishes at once");
    RequireUnsettled([] { sceInvitationDialogClose(); }, "close of a finished dialog");

    Require(sceInvitationDialogGetResult(nullptr) == ArgNull, "null result after the dialog finished");
    InvitationDialogResult result{};
    result.reserved[31] = 1;
    RequireUnsettled([&] { sceInvitationDialogGetResult(&result); }, "result with a non-zero reserved byte");

    InvitationDialogRecipients recipients{};
    recipients.count = 5;
    std::memset(recipients.onlineIds, 'x', sizeof(recipients.onlineIds));
    result = InvitationDialogResult{};
    result.errorCode = 7;
    result.result = 7;
    result.recipients = &recipients;
    Require(sceInvitationDialogGetResult(&result) == 0, "result of the canceled dialog");
    Require(result.callbackArg == &first, "the result carries the callback argument");
    Require(result.errorCode == 0 && result.result == ResultUserCanceled, "the user canceled");
    Require(result.recipients == &recipients && recipients.count == 0, "no invitation was sent");
    Require(recipients.onlineIds[0].data[0] == 'x' && recipients.onlineIds[15].pad[2] == 'x', "the online ids are left alone");
    for (const std::uint8_t value : result.reserved) Require(value == 0, "the reserved bytes stay zero");

    result = InvitationDialogResult{};
    Require(sceInvitationDialogGetResult(&result) == 0 && result.recipients == nullptr, "result without a recipient list");
    errno = 71;
    Require(sceInvitationDialogGetResult(reinterpret_cast<InvitationDialogResult*>(0x10)) == ArgNull, "unwritable result pointer");
    Require(errno == 71, "errno preserved");

    const InvitationDialogParam receive = MakeParam(ModeRecv, &second);
    Require(sceInvitationDialogOpen(&receive) == 0, "open again to receive");
    result = InvitationDialogResult{};
    Require(sceInvitationDialogGetResult(&result) == 0 && result.callbackArg == &second, "the second open carries its own callback argument");

    Require(sceInvitationDialogTerminate() == 0 && !sceCommonDialogIsUsed(), "terminate");
    Require(sceInvitationDialogTerminate() == NotInitialized, "terminate after termination");
    Require(sceInvitationDialogUpdateStatus() == StatusNone, "status after termination");
    Require(sceInvitationDialogGetResult(&result) == NotInitialized, "result after termination");
    Require(sceInvitationDialogGetResult(nullptr) == NotInitialized, "null result after termination");
    Require(sceInvitationDialogClose() == NotInitialized, "close after termination");
    RequireUnsettled([&] { sceInvitationDialogOpen(&send); }, "open after termination");
    RequireUnsettled([] { sceInvitationDialogOpen(nullptr); }, "null open after termination");
    errno = 71;
    RequireUnsettled([] { sceInvitationDialogOpen(reinterpret_cast<const InvitationDialogParam*>(0x10)); }, "unmapped open after termination");
    Require(errno == 71, "errno preserved after termination");
    Require(sceInvitationDialogInitialize() == 0 && sceInvitationDialogGetResult(&result) == NotFinished, "a new initialization forgets the result");
    Require(sceInvitationDialogTerminate() == 0, "terminate again");
}

void TestStatusAndPending() {
    int callbackArg = 0;
    Require(sceInvitationDialogGetStatus_nid_postfix() == StatusNone, "get status before initialization");
    Require(sceInvitationDialogInitialize() == 0, "initialize for status check");
    Require(sceInvitationDialogGetStatus_nid_postfix() == StatusInitialized, "status after initialize");
    const InvitationDialogParam send = MakeParam(ModeSend, &callbackArg);
    Require(sceInvitationDialogOpen(&send) == 0, "open for status check");
    Require(sceInvitationDialogGetStatus_nid_postfix() == StatusFinished, "status after open");
    Require(sceInvitationDialogTerminate() == 0, "terminate after status check");
    Require(sceInvitationDialogGetStatus_nid_postfix() == StatusNone, "status after terminate");
}

void TestVariantA() {
    int first = 0;
    int second = 0;
    const InvitationDialogParamA send = MakeParamA(ModeSend, &first);
    InvitationDialogResultA result{};
    RequireUnsettled([&] { sceInvitationDialogOpenA_nid_postfix(&send); }, "openA before initialization");
    RequireUnsettled([] { sceInvitationDialogOpenA_nid_postfix(nullptr); }, "null openA before initialization");
    Require(sceInvitationDialogGetResultA_nid_postfix(&result) == NotInitialized, "getResultA before initialization");
    Require(sceInvitationDialogGetResultA_nid_postfix(nullptr) == NotInitialized, "null getResultA before initialization");

    Require(sceInvitationDialogInitialize() == 0, "initialize for the A variants");
    Require(sceInvitationDialogGetResultA_nid_postfix(nullptr) == NotFinished, "null getResultA before the dialog finished");
    Require(sceInvitationDialogOpenA_nid_postfix(nullptr) == ArgNull, "openA without a param");
    errno = 71;
    Require(sceInvitationDialogOpenA_nid_postfix(reinterpret_cast<const InvitationDialogParamA*>(0x10)) == ArgNull, "openA with unmapped memory");
    Require(errno == 71, "errno preserved when openA rejects unmapped memory");
    InvitationDialogParamA param = MakeParamA(ModeSend, nullptr);
    param.size = sizeof(InvitationDialogParamA) - 8;
    RequireUnsettled([&] { sceInvitationDialogOpenA_nid_postfix(&param); }, "openA with another param size");
    param = MakeParamA(0, nullptr);
    RequireUnsettled([&] { sceInvitationDialogOpenA_nid_postfix(&param); }, "openA in the invalid mode");
    param = MakeParamA(3, nullptr);
    RequireUnsettled([&] { sceInvitationDialogOpenA_nid_postfix(&param); }, "openA in an unknown mode");
    Require(sceInvitationDialogUpdateStatus() == StatusInitialized, "a rejected openA leaves the status");

    Require(sceInvitationDialogOpenA_nid_postfix(&send) == 0, "openA to send");
    Require(sceInvitationDialogUpdateStatus() == StatusFinished, "the A dialog finishes at once");
    Require(sceInvitationDialogGetResultA_nid_postfix(nullptr) == ArgNull, "null getResultA after the dialog finished");
    result.reserved[0] = 1;
    RequireUnsettled([&] { sceInvitationDialogGetResultA_nid_postfix(&result); }, "getResultA with a non-zero reserved byte");

    InvitationDialogUserList users{};
    users.count = 5;
    users.padding = 0x55555555u;
    for (InvitationDialogUser& user : users.users) {
        std::memset(user.onlineId.data, 'x', sizeof(user.onlineId.data));
        user.accountId = 0x1122334455667788ull;
    }
    result = InvitationDialogResultA{};
    result.errorCode = 7;
    result.result = 7;
    result.sentUsers = &users;
    Require(sceInvitationDialogGetResultA_nid_postfix(&result) == 0, "getResultA of the canceled dialog");
    Require(result.callbackArg == &first, "the A result carries the callback argument");
    Require(result.errorCode == 0 && result.result == ResultUserCanceled, "the user canceled the A dialog");
    Require(result.sentUsers == &users && users.count == 0, "no invitation was sent to an account");
    Require(users.padding == 0x55555555u && users.users[0].onlineId.data[0] == 'x' && users.users[15].accountId == 0x1122334455667788ull, "the user entries are left alone");
    for (const std::uint8_t value : result.reserved) Require(value == 0, "the A reserved bytes stay zero");

    result = InvitationDialogResultA{};
    Require(sceInvitationDialogGetResultA_nid_postfix(&result) == 0 && result.sentUsers == nullptr, "getResultA without a user list");
    result.sentUsers = reinterpret_cast<InvitationDialogUserList*>(0x10);
    errno = 71;
    Require(sceInvitationDialogGetResultA_nid_postfix(&result) == ArgNull, "getResultA with an unwritable user list");
    Require(errno == 71, "errno preserved for an unwritable user list");
    errno = 71;
    Require(sceInvitationDialogGetResultA_nid_postfix(reinterpret_cast<InvitationDialogResultA*>(0x10)) == ArgNull, "unwritable A result pointer");
    Require(errno == 71, "errno preserved for an unwritable A result");

    const InvitationDialogParamA receive = MakeParamA(ModeRecv, &second);
    Require(sceInvitationDialogOpenA_nid_postfix(&receive) == 0, "openA again to receive");
    result = InvitationDialogResultA{};
    Require(sceInvitationDialogGetResultA_nid_postfix(&result) == 0 && result.callbackArg == &second, "the second openA carries its own callback argument");
    const InvitationDialogParam plain = MakeParam(ModeSend, &first);
    Require(sceInvitationDialogOpen(&plain) == 0, "open after openA");
    result = InvitationDialogResultA{};
    Require(sceInvitationDialogGetResultA_nid_postfix(&result) == 0 && result.callbackArg == &first, "getResultA reports the last open of either form");

    Require(sceInvitationDialogTerminate() == 0, "terminate after the A variants");
    Require(sceInvitationDialogGetResultA_nid_postfix(&result) == NotInitialized, "getResultA after termination");
    RequireUnsettled([&] { sceInvitationDialogOpenA_nid_postfix(&send); }, "openA after termination");
}


bool PsnNoticeShown() {
    ModalDialogView view;
    if (!ModalDialogReadView_nid_postfix(view)) return false;
    return view.message == PsnDialogNoticeRequest(PsnDialogNoticeKind::PsnFeature).message;
}

void RequireNotices() {
    const InvitationDialogParam send = MakeParam(ModeSend, nullptr);
    ModalDialogSetPresenter_nid_postfix(true);
    Require(sceInvitationDialogInitialize() == 0, "initialize for the notice check");

    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Silent);
    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    Require(sceInvitationDialogOpen(&send) == 0, "silent open");
    Require(!PsnNoticeShown() && !ModalDialogCapturesInput_nid_postfix(), "silent mode showed a notice");

    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Show);
    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    Require(sceInvitationDialogOpen(nullptr) == ArgNull, "null open");
    Require(!PsnNoticeShown(), "a refused open showed the notice");
    Require(sceInvitationDialogOpen(&send) == 0, "show open");
    Require(PsnNoticeShown(), "open did not show the PSN notice");

    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    ModalDialogSetPresenter_nid_postfix(false);
    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Silent);
    Require(sceInvitationDialogTerminate() == 0, "terminate after the notice check");
}

}

int main() {
    TestBeforeInitialize();
    TestCommonDialogPreconditions();
    TestOpenChecks();
    TestCanceled();
    TestStatusAndPending();
    TestVariantA();
    RequireNotices();
    return 0;
}
