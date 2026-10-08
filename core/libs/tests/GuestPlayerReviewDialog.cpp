#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI scePlayerReviewDialogInitialize(void);
int APS5_VABI scePlayerReviewDialogTerminate(void);
int APS5_VABI scePlayerReviewDialogOpen(const void* param);
int APS5_VABI scePlayerReviewDialogClose(void);
int APS5_VABI scePlayerReviewDialogGetStatus(void);
int APS5_VABI scePlayerReviewDialogUpdateStatus(void);
int APS5_VABI scePlayerReviewDialogGetResult(void* result);
}

namespace {

constexpr int COMMON_DIALOG_STATUS_NONE = 0;
constexpr int COMMON_DIALOG_STATUS_INITIALIZED = 1;
constexpr int COMMON_DIALOG_STATUS_FINISHED = 3;
constexpr int COMMON_DIALOG_RESULT_USER_CANCELED = 1;
constexpr int COMMON_DIALOG_ERROR_NOT_INITIALIZED = static_cast<int>(0x80B80003u);
constexpr int COMMON_DIALOG_ERROR_NOT_FINISHED = static_cast<int>(0x80B80005u);
constexpr int COMMON_DIALOG_ERROR_ARG_NULL = static_cast<int>(0x80B8000Du);

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    std::uint8_t param[64] = {};
    std::int32_t result[8] = {};
    Require(scePlayerReviewDialogGetStatus() == COMMON_DIALOG_STATUS_NONE);
    Require(scePlayerReviewDialogOpen(param) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(scePlayerReviewDialogClose() == COMMON_DIALOG_ERROR_NOT_INITIALIZED);
    Require(scePlayerReviewDialogGetResult(result) == COMMON_DIALOG_ERROR_NOT_INITIALIZED);

    Require(scePlayerReviewDialogInitialize() == 0);
    Require(scePlayerReviewDialogGetStatus() == COMMON_DIALOG_STATUS_INITIALIZED);
    Require(scePlayerReviewDialogUpdateStatus() == COMMON_DIALOG_STATUS_INITIALIZED);
    Require(scePlayerReviewDialogGetResult(result) == COMMON_DIALOG_ERROR_NOT_FINISHED);
    Require(scePlayerReviewDialogTerminate() == 0);
    Require(scePlayerReviewDialogGetStatus() == COMMON_DIALOG_STATUS_NONE);

    Require(scePlayerReviewDialogInitialize() == 0);
    Require(scePlayerReviewDialogOpen(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(scePlayerReviewDialogOpen(param) == 0);
    Require(scePlayerReviewDialogGetStatus() == COMMON_DIALOG_STATUS_FINISHED);
    Require(scePlayerReviewDialogUpdateStatus() == COMMON_DIALOG_STATUS_FINISHED);
    Require(scePlayerReviewDialogGetResult(nullptr) == COMMON_DIALOG_ERROR_ARG_NULL);
    Require(scePlayerReviewDialogGetResult(result) == 0);
    Require(result[0] == COMMON_DIALOG_RESULT_USER_CANCELED);
    Require(scePlayerReviewDialogClose() == 0);
    Require(scePlayerReviewDialogTerminate() == 0);
    Require(scePlayerReviewDialogGetStatus() == COMMON_DIALOG_STATUS_NONE);
    Require(scePlayerReviewDialogInitialize() == 0);
    Require(scePlayerReviewDialogTerminate() == 0);
}
