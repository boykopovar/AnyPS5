#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" {
int APS5_VABI scePs2EmuMenuDialogInitialize(void);
int APS5_VABI scePs2EmuMenuDialogOpen(const void* param);
int APS5_VABI scePs2EmuMenuDialogUpdateStatus(void);
int APS5_VABI scePs2EmuMenuDialogGetStatus(void);
int APS5_VABI scePs2EmuMenuDialogGetResult(void* result);
int APS5_VABI scePs2EmuMenuDialogClose(void);
int APS5_VABI scePs2EmuMenuDialogTerminate(void);
}

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "PS2EmuMenuDialog: %s\n", message);
        std::abort();
    }
}

constexpr int kStatusNone = 0;
constexpr int kStatusInitialized = 1;
constexpr int kStatusFinished = 3;
constexpr int kErrNotInitialized = static_cast<int>(0x80B80003);
constexpr int kErrAlreadyInitialized = static_cast<int>(0x80B80004);
constexpr int kErrNotFinished = static_cast<int>(0x80B80005);
constexpr int kErrNotRunning = static_cast<int>(0x80B8000B);
constexpr int kErrArgNull = static_cast<int>(0x80B8000D);
constexpr int kResultUserCanceled = 1;

}

int main() {
    std::int64_t param = 0;
    std::int32_t result[2] = {-1, -1};

    Require(scePs2EmuMenuDialogTerminate() == kErrNotInitialized, "terminate before init");
    Require(scePs2EmuMenuDialogOpen(&param) == kErrNotInitialized, "open before init");
    Require(scePs2EmuMenuDialogClose() == kErrNotInitialized, "close before init");
    Require(scePs2EmuMenuDialogGetResult(result) == kErrNotInitialized, "get result before init");
    Require(scePs2EmuMenuDialogGetStatus() == kStatusNone, "status before init");
    Require(scePs2EmuMenuDialogUpdateStatus() == kStatusNone, "update status before init");

    Require(scePs2EmuMenuDialogInitialize() == 0, "initialize");
    Require(scePs2EmuMenuDialogInitialize() == kErrAlreadyInitialized, "double initialize");
    Require(scePs2EmuMenuDialogGetStatus() == kStatusInitialized, "status after initialize");
    Require(scePs2EmuMenuDialogUpdateStatus() == kStatusInitialized, "update status after initialize");

    Require(scePs2EmuMenuDialogClose() == kErrNotRunning, "close while initialized");
    Require(scePs2EmuMenuDialogOpen(nullptr) == kErrArgNull, "open with null param");
    Require(scePs2EmuMenuDialogGetResult(result) == kErrNotFinished, "get result before open");

    Require(scePs2EmuMenuDialogTerminate() == 0, "terminate from initialized");
    Require(scePs2EmuMenuDialogTerminate() == kErrNotInitialized, "terminate after terminate");
    Require(scePs2EmuMenuDialogGetStatus() == kStatusNone, "status after terminate");

    Require(scePs2EmuMenuDialogInitialize() == 0, "re-initialize");
    Require(scePs2EmuMenuDialogOpen(&param) == 0, "open");
    Require(scePs2EmuMenuDialogGetStatus() == kStatusFinished, "status after open");
    Require(scePs2EmuMenuDialogUpdateStatus() == kStatusFinished, "update status after open");

    Require(scePs2EmuMenuDialogGetResult(nullptr) == kErrArgNull, "get result null pointer");
    Require(scePs2EmuMenuDialogGetResult(result) == 0, "get result");
    Require(result[0] == kResultUserCanceled, "result canceled code");
    Require(result[1] == -1, "result untouched buffer");

    Require(scePs2EmuMenuDialogClose() == kErrNotRunning, "close when finished");
    Require(scePs2EmuMenuDialogTerminate() == 0, "terminate from finished");
    Require(scePs2EmuMenuDialogTerminate() == kErrNotInitialized, "terminate after terminate 2");
    Require(scePs2EmuMenuDialogGetStatus() == kStatusNone, "status after terminate 2");
    Require(scePs2EmuMenuDialogUpdateStatus() == kStatusNone, "update status after terminate 2");
}
