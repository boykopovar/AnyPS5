#include "prx/libSceCommonDialog/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "prx/libSceCommonDialog/include/DialogUse.hpp"
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

constexpr int kErrAlreadySystemInitialized = static_cast<int>(0x80B80002);

static_assert(COMMON_DIALOG_OK == 0);
static_assert(COMMON_DIALOG_STATUS_NONE == 0 && COMMON_DIALOG_STATUS_INITIALIZED == 1 && COMMON_DIALOG_STATUS_RUNNING == 2 && COMMON_DIALOG_STATUS_FINISHED == 3);
static_assert(COMMON_DIALOG_RESULT_OK == 0 && COMMON_DIALOG_RESULT_USER_CANCELED == 1);
static_assert(COMMON_DIALOG_ERROR_NOT_SYSTEM_INITIALIZED == static_cast<int>(0x80B80001u));
static_assert(COMMON_DIALOG_ERROR_ALREADY_SYSTEM_INITIALIZED == static_cast<int>(0x80B80002u));
static_assert(COMMON_DIALOG_ERROR_NOT_INITIALIZED == static_cast<int>(0x80B80003u));
static_assert(COMMON_DIALOG_ERROR_ALREADY_INITIALIZED == static_cast<int>(0x80B80004u));
static_assert(COMMON_DIALOG_ERROR_NOT_FINISHED == static_cast<int>(0x80B80005u));
static_assert(COMMON_DIALOG_ERROR_INVALID_STATE == static_cast<int>(0x80B80006u));
static_assert(COMMON_DIALOG_ERROR_INVALID_ARGUMENT == static_cast<int>(0x80B8000Au) && COMMON_DIALOG_ERROR_PARAMETER == static_cast<int>(0x80B8000Au));
static_assert(COMMON_DIALOG_ERROR_NOT_RUNNING == static_cast<int>(0x80B8000Bu));
static_assert(COMMON_DIALOG_ERROR_ARG_NULL == static_cast<int>(0x80B8000Du));

}

int main() {
    Require(!sceCommonDialogIsUsed());
    Require(!CommonDialogSystemInitialized_nid_no_patch());
    Require(sceCommonDialogInitialize() == COMMON_DIALOG_OK);
    Require(CommonDialogSystemInitialized_nid_no_patch());
    Require(!sceCommonDialogIsUsed());
    Require(sceCommonDialogInitialize() == kErrAlreadySystemInitialized);
    Require(sceCommonDialogInitialize() == kErrAlreadySystemInitialized);
    Require(!sceCommonDialogIsUsed());
    Require(CommonDialogAcquire_nid_no_patch() && sceCommonDialogIsUsed());
    Require(!CommonDialogAcquire_nid_no_patch() && sceCommonDialogIsUsed());
    CommonDialogRelease_nid_no_patch();
    Require(!sceCommonDialogIsUsed());
    Require(CommonDialogAcquire_nid_no_patch());
    CommonDialogRelease_nid_no_patch();

    CommonDialogBaseParam base{};
    base.size = sizeof(base);
    base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&base)) - 1060003575u;
    Require(ValidateCommonDialogBaseParam(base));
    base.size = sizeof(base) - 1;
    Require(!ValidateCommonDialogBaseParam(base));
    base.size = sizeof(base);
    base.magic = 0;
    Require(!ValidateCommonDialogBaseParam(base));
    base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&base)) - 1060003575u;
    base.reserved[0] = 1;
    Require(!ValidateCommonDialogBaseParam(base));
    base.reserved[0] = 0;
    base.reserved[35] = 1;
    Require(!ValidateCommonDialogBaseParam(base));
    base.reserved[35] = 0;
    Require(ValidateCommonDialogBaseParam(base));
}
