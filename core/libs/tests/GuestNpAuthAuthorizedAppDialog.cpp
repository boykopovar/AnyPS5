#include "prx/libSceNpAuthAuthorizedAppDialog/include/Api.hpp"
#include "prx/libSceIme/include/ModalDialog.hpp"
#include "prx/libSceIme/include/PsnDialogNotice.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <string>
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "prx/libSceUserService/UserService.hpp"

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
    NpAuthAuthorizedAppDialogParam param{};
    param.base.size = sizeof(param.base);
    param.base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&param)) - 1060003575u;
    param.size = sizeof(param);
    param.userId = USER_SERVICE_INITIAL_USER_ID;
    char application[136] = "test";
    std::string text(16, 'a');
    param.application = application;
    param.text = text.c_str();

    ModalDialogSetPresenter_nid_postfix(true);
    Require(sceNpAuthAuthorizedAppDialogInitialize() == 0);

    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Silent);
    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == 0);
    Require(!PsnNoticeShown() && !ModalDialogCapturesInput_nid_postfix());

    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Show);
    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    Require(sceNpAuthAuthorizedAppDialogOpen(nullptr) == static_cast<int>(0x80B8000Du));
    Require(!PsnNoticeShown());
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == 0);
    Require(PsnNoticeShown());

    ModalDialogReset_nid_postfix(ModalDialogAnyOwner);
    ModalDialogSetPresenter_nid_postfix(false);
    PsnDialogNoticeSetMode_nid_postfix(PsnDialogNoticeMode::Silent);
    Require(sceNpAuthAuthorizedAppDialogTerminate() == 0);
}

}

int main() {
    Require(sceUserServiceInitialize(nullptr) == 0);
    NpAuthAuthorizedAppDialogParam param{};
    param.base.size = sizeof(param.base);
    param.base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&param)) - 1060003575u;
    param.size = sizeof(param);
    param.userId = USER_SERVICE_INITIAL_USER_ID;
    char application[136] = "test";
    std::string text(1024, 'a');
    param.application = application;
    param.text = text.c_str();
    std::int32_t result[2] = {-1, -1};

    Require(sceNpAuthAuthorizedAppDialogGetStatus() == kStatusNone);
    Require(sceNpAuthAuthorizedAppDialogUpdateStatus() == kStatusNone);
    Require(sceNpAuthAuthorizedAppDialogTerminate() == kErrNotInitialized);
    Require(sceNpAuthAuthorizedAppDialogClose() == kErrNotInitialized);
    Require(sceNpAuthAuthorizedAppDialogOpen(nullptr) == kErrArgNull);
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == kErrNotInitialized);
    Require(sceNpAuthAuthorizedAppDialogGetResult(nullptr) == kErrArgNull);
    Require(sceNpAuthAuthorizedAppDialogGetResult(result) == kErrNotInitialized);

    constexpr int invalidArgument = static_cast<int>(0x80B8000Au);
    param.base.size = 0;
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == invalidArgument);
    param.base.size = sizeof(param.base);
    param.base.magic = 0;
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == invalidArgument);
    param.base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&param)) - 1060003575u;
    param.size = 0;
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == invalidArgument);
    param.size = sizeof(param);
    param.userId = -1;
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == invalidArgument);
    param.userId = 42;
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == invalidArgument);
    param.userId = USER_SERVICE_INITIAL_USER_ID;
    param.application = nullptr;
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == invalidArgument);
    application[0] = 0;
    param.application = application;
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == invalidArgument);
    application[0] = 't';
    param.text = nullptr;
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == invalidArgument);
    param.text = "";
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == invalidArgument);
    text.push_back('a');
    param.text = text.c_str();
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == invalidArgument);
    text.pop_back();
    param.text = text.c_str();
    param.mode = 2;
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == invalidArgument);
    param.mode = 1;
    param.reserved[31] = 1;
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == invalidArgument);
    param.reserved[31] = 0;
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == kErrNotInitialized);

    Require(sceNpAuthAuthorizedAppDialogInitialize() == 0);
    Require(sceNpAuthAuthorizedAppDialogInitialize() == kErrAlreadyInitialized);
    Require(sceNpAuthAuthorizedAppDialogGetStatus() == kStatusInitialized);
    Require(sceNpAuthAuthorizedAppDialogUpdateStatus() == kStatusInitialized);

    Require(sceNpAuthAuthorizedAppDialogOpen(nullptr) == kErrArgNull);
    Require(sceNpAuthAuthorizedAppDialogGetResult(result) == kErrNotFinished);

    param.size = 119;
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == invalidArgument);
    param.size = sizeof(param);
    param.userId = 42;
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == invalidArgument);
    param.userId = USER_SERVICE_INITIAL_USER_ID;
    param.application = nullptr;
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == invalidArgument);
    param.application = application;
    param.text = "";
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == invalidArgument);
    text.push_back('a');
    param.text = text.c_str();
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == invalidArgument);
    text.pop_back();
    param.text = text.c_str();
    param.mode = 2;
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == invalidArgument);
    param.mode = 1;
    param.reserved[31] = 1;
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == invalidArgument);
    param.reserved[31] = 0;
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == 0);
    Require(sceNpAuthAuthorizedAppDialogUpdateStatus() == kStatusFinished);
    Require(sceNpAuthAuthorizedAppDialogGetStatus() == kStatusFinished);

    Require(sceNpAuthAuthorizedAppDialogGetResult(nullptr) == kErrArgNull);
    Require(sceNpAuthAuthorizedAppDialogGetResult(result) == kResultUserCanceled);
    Require(result[0] == kResultUserCanceled);
    Require(result[1] == -1);

    Require(sceNpAuthAuthorizedAppDialogClose() == static_cast<int>(0x80B8000Bu));
    Require(sceNpAuthAuthorizedAppDialogTerminate() == 0);
    Require(sceNpAuthAuthorizedAppDialogTerminate() == kErrNotInitialized);
    Require(sceNpAuthAuthorizedAppDialogClose() == kErrNotInitialized);
    Require(sceNpAuthAuthorizedAppDialogOpen(nullptr) == kErrArgNull);
    Require(sceNpAuthAuthorizedAppDialogOpen(&param) == kErrNotInitialized);
    Require(sceNpAuthAuthorizedAppDialogGetResult(nullptr) == kErrArgNull);
    Require(sceNpAuthAuthorizedAppDialogGetResult(result) == kErrNotInitialized);
    Require(sceNpAuthAuthorizedAppDialogGetStatus() == kStatusNone);
    Require(sceNpAuthAuthorizedAppDialogUpdateStatus() == kStatusNone);

    Require(sceNpAuthAuthorizedAppDialogInitialize() == 0);
    Require(sceNpAuthAuthorizedAppDialogGetStatus() == kStatusInitialized);
    Require(sceNpAuthAuthorizedAppDialogClose() == static_cast<int>(0x80B8000Bu));
    Require(sceNpAuthAuthorizedAppDialogGetStatus() == kStatusInitialized);
    Require(sceNpAuthAuthorizedAppDialogGetResult(result) == kErrNotFinished);
    Require(sceNpAuthAuthorizedAppDialogTerminate() == 0);
    Require(sceNpAuthAuthorizedAppDialogTerminate() == kErrNotInitialized);
    RequireNotices();
    Require(sceUserServiceTerminate() == 0);
    return 0;
}
