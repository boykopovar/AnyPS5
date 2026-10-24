#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include <cstdlib>

extern "C" {
int APS5_VABI sceCommonDialogInitialize(void);
bool APS5_VABI sceCommonDialogIsUsed(void);
}

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

constexpr int kErrAlreadySystemInitialized = static_cast<int>(0x80B80002);

}

int main() {
    Require(!sceCommonDialogIsUsed());
    Require(sceCommonDialogInitialize() == COMMON_DIALOG_OK);
    Require(!sceCommonDialogIsUsed());
    Require(sceCommonDialogInitialize() == kErrAlreadySystemInitialized);
    Require(sceCommonDialogInitialize() == kErrAlreadySystemInitialized);
    Require(!sceCommonDialogIsUsed());
}
