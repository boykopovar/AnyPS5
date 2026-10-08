#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI sceVrSetupDialogInitialize(void);
int APS5_VABI sceVrSetupDialogOpen(const void* param);
int APS5_VABI sceVrSetupDialogUpdateStatus(void);
int APS5_VABI sceVrSetupDialogGetResult(void* result);
int APS5_VABI sceVrSetupDialogClose(void);
int APS5_VABI sceVrSetupDialogTerminate(void);
}

namespace {

constexpr int COMMON_DIALOG_STATUS_NONE = 0;
constexpr int COMMON_DIALOG_STATUS_INITIALIZED = 1;
constexpr int COMMON_DIALOG_STATUS_FINISHED = 3;
constexpr int COMMON_DIALOG_RESULT_USER_CANCELED = 1;
constexpr int COMMON_DIALOG_ERROR_NOT_INITIALIZED = static_cast<int>(0x80B80003u);
constexpr int COMMON_DIALOG_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80B80004u);
constexpr int COMMON_DIALOG_ERROR_NOT_FINISHED = static_cast<int>(0x80B80005u);
constexpr int COMMON_DIALOG_ERROR_ARG_NULL = static_cast<int>(0x80B8000Du);
constexpr std::int32_t UNTOUCHED = 0x5A5A5A5A;

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

}

int main() {
    std::uint8_t param[0x68] = {};
    std::int32_t result[9] = {UNTOUCHED, UNTOUCHED, UNTOUCHED, UNTOUCHED, UNTOUCHED, UNTOUCHED, UNTOUCHED, UNTOUCHED, UNTOUCHED};
    Require(sceVrSetupDialogUpdateStatus() == COMMON_DIALOG_STATUS_NONE);
    Require(sceVrSetupDialogOpen(param) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceVrSetupDialogGetResult(result) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceVrSetupDialogClose() == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceVrSetupDialogTerminate() == COMMON_DIALOG_ERROR_NOT_INITIALIZED);

    Require(sceVrSetupDialogInitialize() == 0);
    Require(sceVrSetupDialogInitialize() == COMMON_DIALOG_ERROR_ALREADY_INITIALIZED);
    Require(sceVrSetupDialogUpdateStatus() == COMMON_DIALOG_STATUS_INITIALIZED);
    Require(sceVrSetupDialogClose() == 0);
    Require(sceVrSetupDialogUpdateStatus() == COMMON_DIALOG_STATUS_INITIALIZED);
    Require(sceVrSetupDialogGetResult(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(sceVrSetupDialogGetResult(result) == COMMON_DIALOG_ERROR_NOT_FINISHED);
    Require(result[0] == UNTOUCHED);
    Require(sceVrSetupDialogOpen(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(sceVrSetupDialogUpdateStatus() == COMMON_DIALOG_STATUS_INITIALIZED);
    Require(sceVrSetupDialogTerminate() == 0);
    Require(sceVrSetupDialogUpdateStatus() == COMMON_DIALOG_STATUS_NONE);

    Require(sceVrSetupDialogInitialize() == 0);
    Require(sceVrSetupDialogOpen(param) == 0);
    Require(sceVrSetupDialogUpdateStatus() == COMMON_DIALOG_STATUS_FINISHED);
    Require(sceVrSetupDialogGetResult(result) == 0);
    Require(result[0] == COMMON_DIALOG_RESULT_USER_CANCELED);
    for (int i = 1; i < 9; ++i) {
        Require(result[i] == UNTOUCHED);
    }
    Require(sceVrSetupDialogOpen(param) == 0);
    Require(sceVrSetupDialogUpdateStatus() == COMMON_DIALOG_STATUS_FINISHED);
    Require(sceVrSetupDialogClose() == 0);
    Require(sceVrSetupDialogTerminate() == 0);
    Require(sceVrSetupDialogUpdateStatus() == COMMON_DIALOG_STATUS_NONE);
    Require(sceVrSetupDialogTerminate() == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(sceVrSetupDialogClose() == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
}
