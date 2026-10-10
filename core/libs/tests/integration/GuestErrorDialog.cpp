#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI sceErrorDialogInitialize(void);
int APS5_VABI sceErrorDialogOpen(const void* param);
int APS5_VABI sceErrorDialogUpdateStatus(void);
int APS5_VABI sceErrorDialogGetStatus(void);
int APS5_VABI sceErrorDialogClose(void);
int APS5_VABI sceErrorDialogTerminate(void);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kStatusRunning = 2;
constexpr int kStatusFinished = 3;
constexpr int kErrNotInitialized = static_cast<int>(0x80ED0001);
constexpr int kErrAlreadyInitialized = static_cast<int>(0x80ED0002);
constexpr int kErrParam = static_cast<int>(0x80ED0003);
constexpr int kErrInvalidState = static_cast<int>(0x80ED0005);

struct ErrorDialogParam {
    std::int32_t size;
    std::int32_t error_code;
    std::int32_t user_id;
    std::int32_t reserved;
};

}

int main() {
    ErrorDialogParam param{16, static_cast<std::int32_t>(0x80020001), 0, 0};

    Require(sceErrorDialogTerminate() == kErrNotInitialized);
    Require(sceErrorDialogOpen(&param) == kErrInvalidState);

    Require(sceErrorDialogInitialize() == 0);
    Require(sceErrorDialogInitialize() == kErrAlreadyInitialized);

    Require(sceErrorDialogClose() == kErrInvalidState);

    Require(sceErrorDialogOpen(nullptr) == kErrParam);
    ErrorDialogParam bad{8, 0, 0, 0};
    Require(sceErrorDialogOpen(&bad) == kErrParam);

    Require(sceErrorDialogOpen(&param) == 0);
    Require(sceErrorDialogGetStatus() == kStatusRunning);
    Require(sceErrorDialogOpen(&param) == kErrInvalidState);

    Require(sceErrorDialogUpdateStatus() == kStatusFinished);
    Require(sceErrorDialogGetStatus() == kStatusFinished);

    Require(sceErrorDialogOpen(&param) == 0);
    Require(sceErrorDialogGetStatus() == kStatusRunning);
    Require(sceErrorDialogClose() == 0);
    Require(sceErrorDialogGetStatus() == kStatusFinished);

    Require(sceErrorDialogTerminate() == 0);
    Require(sceErrorDialogTerminate() == kErrNotInitialized);
}
