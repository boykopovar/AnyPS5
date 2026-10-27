#include "prx/libSceVrSetupDialog/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include "prx/libSceCommonDialog/CommonDialog.hpp"

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kStatusNone = 0;
constexpr int kStatusInitialized = 1;
constexpr int kStatusFinished = 3;
constexpr int kErrNotInitialized = static_cast<int>(0x8A7C0001);
constexpr int kErrAlreadyInitialized = static_cast<int>(0x8A7C0002);
constexpr int kErrNotFinished = static_cast<int>(0x8A7C0005);
constexpr int kErrArgNull = static_cast<int>(0x8A7C0003);
constexpr int kResultUserCanceled = 1;

}

int main() {
    VrSetupDialogParam param{};
    param.size = sizeof(param);
    std::int32_t result[2] = {-1, -1};

    Require(sceVrSetupDialogTerminate() == kErrNotInitialized);
    Require(sceVrSetupDialogOpen(nullptr) == kErrArgNull);
    Require(sceVrSetupDialogOpen(&param) == kErrNotInitialized);
    Require(sceVrSetupDialogClose() == kErrNotInitialized);
    Require(sceVrSetupDialogGetResult(nullptr) == kErrArgNull);
    Require(sceVrSetupDialogGetResult(result) == kErrNotInitialized);
    Require(result[0] == -1 && result[1] == -1);
    Require(sceVrSetupDialogGetStatus() == kStatusNone);
    Require(sceVrSetupDialogUpdateStatus() == kStatusNone);

    Require(sceVrSetupDialogInitialize() == 0);
    Require(sceVrSetupDialogInitialize() == kErrAlreadyInitialized);
    Require(sceVrSetupDialogGetStatus() == kStatusInitialized);
    Require(sceVrSetupDialogUpdateStatus() == kStatusInitialized);

    Require(sceVrSetupDialogOpen(nullptr) == static_cast<int>(0x8A7C0003u));
    Require(sceVrSetupDialogGetResult(result) == kErrNotFinished);
    Require(result[0] == kResultUserCanceled && result[1] == -1);

    param.size = 55;
    Require(sceVrSetupDialogOpen(&param) == static_cast<int>(0x8A7C0003u));
    param.size = sizeof(param);
    param.reserved8 = 1;
    Require(sceVrSetupDialogOpen(&param) == static_cast<int>(0x8A7C0003u));
    param.reserved8 = 0;
    param.reserved9 = 1;
    Require(sceVrSetupDialogOpen(&param) == static_cast<int>(0x8A7C0003u));
    param.reserved9 = 0;
    param.data[45] = 255;
    Require(sceVrSetupDialogOpen(&param) == 0);
    Require(sceVrSetupDialogGetStatus() == kStatusFinished);
    Require(sceVrSetupDialogUpdateStatus() == kStatusFinished);

    Require(sceVrSetupDialogGetResult(nullptr) == kErrArgNull);
    Require(sceVrSetupDialogGetResult(result) == 0);
    Require(result[0] == kResultUserCanceled);
    Require(result[1] == -1);

    Require(sceVrSetupDialogClose() == 0);
    Require(sceVrSetupDialogTerminate() == 0);
    Require(sceVrSetupDialogTerminate() == kErrNotInitialized);
    Require(sceVrSetupDialogOpen(nullptr) == kErrArgNull);
    Require(sceVrSetupDialogGetResult(nullptr) == kErrArgNull);
    Require(sceVrSetupDialogGetStatus() == kStatusNone);
    Require(sceVrSetupDialogUpdateStatus() == kStatusNone);

    Require(sceVrSetupDialogInitialize() == 0);
    Require(sceVrSetupDialogUpdateStatus() == kStatusInitialized);
    Require(sceVrSetupDialogClose() == 0);
    Require(sceVrSetupDialogUpdateStatus() == kStatusInitialized);
    Require(sceVrSetupDialogGetResult(result) == kErrNotFinished);
    Require(result[0] == kResultUserCanceled && result[1] == -1);
    Require(sceVrSetupDialogTerminate() == 0);
}
