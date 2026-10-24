#include "prx/libSceVrServiceDialog/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

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

}

int main() {
    std::int64_t param = 0;
    std::int32_t result[2] = {-1, -1};

    Require(sceVrServiceDialogTerminate() == kErrNotInitialized);
    Require(sceVrServiceDialogOpen(&param) == kErrNotInitialized);
    Require(sceVrServiceDialogClose() == kErrNotInitialized);
    Require(sceVrServiceDialogGetResult(result) == kErrNotInitialized);
    Require(sceVrServiceDialogGetStatus() == kStatusNone);
    Require(sceVrServiceDialogUpdateStatus() == kStatusNone);

    Require(sceVrServiceDialogInitialize() == 0);
    Require(sceVrServiceDialogInitialize() == kErrAlreadyInitialized);
    Require(sceVrServiceDialogGetStatus() == kStatusInitialized);
    Require(sceVrServiceDialogUpdateStatus() == kStatusInitialized);

    Require(sceVrServiceDialogOpen(nullptr) == kErrArgNull);
    Require(sceVrServiceDialogGetResult(result) == kErrNotFinished);

    Require(sceVrServiceDialogOpen(&param) == 0);
    Require(sceVrServiceDialogGetStatus() == kStatusFinished);
    Require(sceVrServiceDialogUpdateStatus() == kStatusFinished);

    Require(sceVrServiceDialogGetResult(nullptr) == kErrArgNull);
    Require(sceVrServiceDialogGetResult(result) == 0);
    Require(result[0] == kResultUserCanceled);
    Require(result[1] == -1);

    Require(sceVrServiceDialogClose() == 0);
    Require(sceVrServiceDialogTerminate() == 0);
    Require(sceVrServiceDialogTerminate() == kErrNotInitialized);
    Require(sceVrServiceDialogGetStatus() == kStatusNone);
    Require(sceVrServiceDialogUpdateStatus() == kStatusNone);

    Require(sceVrServiceDialogInitialize() == 0);
    Require(sceVrServiceDialogUpdateStatus() == kStatusInitialized);
    Require(sceVrServiceDialogClose() == 0);
    Require(sceVrServiceDialogUpdateStatus() == kStatusFinished);
    result[0] = -1;
    Require(sceVrServiceDialogGetResult(result) == 0);
    Require(result[0] == kResultUserCanceled);
    Require(sceVrServiceDialogTerminate() == 0);
}
