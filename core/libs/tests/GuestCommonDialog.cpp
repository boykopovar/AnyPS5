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
