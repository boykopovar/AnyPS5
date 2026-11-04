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
}
