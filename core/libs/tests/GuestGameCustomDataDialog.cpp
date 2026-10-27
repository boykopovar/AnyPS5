#include "prx/libSceGameCustomDataDialog/include/Api.hpp"
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

    Require(sceGameCustomDataDialogTerminate() == kErrNotInitialized);
    Require(sceGameCustomDataDialogOpenA(nullptr) == kErrNotInitialized);
    Require(sceGameCustomDataDialogOpenA(&param) == kErrNotInitialized);
    Require(sceGameCustomDataDialogClose() == kErrNotInitialized);
    Require(sceGameCustomDataDialogGetResultA(nullptr) == kErrNotInitialized);
    Require(sceGameCustomDataDialogGetResultA(result) == kErrNotInitialized);
    Require(sceGameCustomDataDialogGetStatus() == kStatusNone);
    Require(sceGameCustomDataDialogUpdateStatus() == kStatusNone);

    Require(sceGameCustomDataDialogInitialize() == 0);
    Require(sceGameCustomDataDialogInitialize() == kErrAlreadyInitialized);
    Require(sceGameCustomDataDialogGetStatus() == kStatusInitialized);
    Require(sceGameCustomDataDialogUpdateStatus() == kStatusInitialized);

    Require(sceGameCustomDataDialogOpenA(nullptr) == kErrArgNull);
    Require(sceGameCustomDataDialogGetResultA(nullptr) == kErrArgNull);
    Require(sceGameCustomDataDialogGetResultA(result) == kErrNotFinished);

    Require(sceGameCustomDataDialogOpenA(&param) == 0);
    Require(sceGameCustomDataDialogGetStatus() == kStatusFinished);
    Require(sceGameCustomDataDialogUpdateStatus() == kStatusFinished);

    Require(sceGameCustomDataDialogGetResultA(nullptr) == kErrArgNull);
    Require(sceGameCustomDataDialogGetResultA(result) == 0);
    Require(result[0] == kResultUserCanceled);
    Require(result[1] == -1);

    Require(sceGameCustomDataDialogClose() == 0);
    Require(sceGameCustomDataDialogTerminate() == 0);
    Require(sceGameCustomDataDialogTerminate() == kErrNotInitialized);
    Require(sceGameCustomDataDialogOpenA(nullptr) == kErrNotInitialized);
    Require(sceGameCustomDataDialogGetResultA(nullptr) == kErrNotInitialized);
    Require(sceGameCustomDataDialogGetStatus() == kStatusNone);
    Require(sceGameCustomDataDialogUpdateStatus() == kStatusNone);

    Require(sceGameCustomDataDialogInitialize() == 0);
    Require(sceGameCustomDataDialogUpdateStatus() == kStatusInitialized);
    Require(sceGameCustomDataDialogClose() == 0);
    Require(sceGameCustomDataDialogUpdateStatus() == kStatusFinished);
    result[0] = -1;
    Require(sceGameCustomDataDialogGetResultA(result) == 0);
    Require(result[0] == kResultUserCanceled);
    Require(sceGameCustomDataDialogTerminate() == 0);
}
