#include "prx/libSceInvitationDialog/include/Api.hpp"
#include "prx/libSceCommonDialog/include/Api.hpp"
#include "prx/libSceCommonDialog/include/DialogUse.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
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

void TestBeforeInitialize() {
    InvitationDialogResult result{};
    Require(sceInvitationDialogUpdateStatus() == StatusNone, "status before initialization");
    Require(sceInvitationDialogGetResult(&result) == NotInitialized, "result before initialization");
    Require(sceInvitationDialogGetResult(nullptr) == NotInitialized, "null result before initialization");
    Require(sceInvitationDialogClose() == NotInitialized, "close before initialization");
    Require(sceInvitationDialogTerminate() == NotInitialized, "terminate before initialization");
    const InvitationDialogParam param = MakeParam(ModeSend, nullptr);
    RequireUnsettled([&] { sceInvitationDialogOpen(&param); }, "open before initialization");
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

    const InvitationDialogParam receive = MakeParam(ModeRecv, &second);
    Require(sceInvitationDialogOpen(&receive) == 0, "open again to receive");
    result = InvitationDialogResult{};
    Require(sceInvitationDialogGetResult(&result) == 0 && result.callbackArg == &second, "the second open carries its own callback argument");

    Require(sceInvitationDialogTerminate() == 0 && !sceCommonDialogIsUsed(), "terminate");
    Require(sceInvitationDialogUpdateStatus() == StatusNone, "status after termination");
    Require(sceInvitationDialogGetResult(&result) == NotInitialized, "result after termination");
    Require(sceInvitationDialogInitialize() == 0 && sceInvitationDialogGetResult(&result) == NotFinished, "a new initialization forgets the result");
    Require(sceInvitationDialogTerminate() == 0, "terminate again");
}

template <typename TCall>
void RequireNotImplemented(TCall call, const char* message) {
    bool threw = false;
    try {
        call();
    } catch (const std::runtime_error& error) {
        threw = std::string(error.what()).find("not implemented") != std::string::npos;
    }
    Require(threw, message);
}

void TestStatusAndPending() {
    int callbackArg = 0;
    Require(sceInvitationDialogGetStatus_nid_postfix() == StatusNone, "get status before initialization");
    RequireNotImplemented([] { sceInvitationDialogOpenA_nid_postfix(); }, "openA is not implemented");
    RequireNotImplemented([] { sceInvitationDialogGetResultA_nid_postfix(); }, "getResultA is not implemented");
    Require(sceInvitationDialogInitialize() == 0, "initialize for status check");
    Require(sceInvitationDialogGetStatus_nid_postfix() == StatusInitialized, "status after initialize");
    const InvitationDialogParam send = MakeParam(ModeSend, &callbackArg);
    Require(sceInvitationDialogOpen(&send) == 0, "open for status check");
    Require(sceInvitationDialogGetStatus_nid_postfix() == StatusFinished, "status after open");
    Require(sceInvitationDialogTerminate() == 0, "terminate after status check");
    Require(sceInvitationDialogGetStatus_nid_postfix() == StatusNone, "status after terminate");
}

}

int main() {
    TestBeforeInitialize();
    TestCommonDialogPreconditions();
    TestOpenChecks();
    TestCanceled();
    TestStatusAndPending();
    return 0;
}
