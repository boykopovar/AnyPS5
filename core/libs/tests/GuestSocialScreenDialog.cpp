#include "prx/libSceSocialScreenDialog/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

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
constexpr std::int32_t kSentinel = static_cast<std::int32_t>(0x5a5a5a5a);

}

int main() {
    std::int64_t param = 0;
    std::int32_t result[2];
    std::memset(result, 0x5a, sizeof(result));

    Require(sceSocialScreenDialogTerminate() == kErrNotInitialized);
    Require(sceSocialScreenDialogOpen(nullptr) == kErrNotInitialized);
    Require(sceSocialScreenDialogOpen(&param) == kErrNotInitialized);
    Require(sceSocialScreenDialogClose() == kErrNotInitialized);
    Require(sceSocialScreenDialogGetResult(nullptr) == kErrNotInitialized);
    Require(sceSocialScreenDialogGetResult(result) == kErrNotInitialized);
    Require(sceSocialScreenDialogGetStatus() == kStatusNone);
    Require(sceSocialScreenDialogUpdateStatus() == kStatusNone);

    Require(sceSocialScreenDialogInitialize() == 0);
    Require(sceSocialScreenDialogInitialize() == kErrAlreadyInitialized);
    Require(sceSocialScreenDialogGetStatus() == kStatusInitialized);
    Require(sceSocialScreenDialogUpdateStatus() == kStatusInitialized);

    Require(sceSocialScreenDialogOpen(nullptr) == kErrArgNull);
    std::memset(result, 0x5a, sizeof(result));
    Require(sceSocialScreenDialogGetResult(nullptr) == kErrArgNull);
    Require(sceSocialScreenDialogGetResult(result) == kErrNotFinished);
    Require(result[0] == kSentinel);
    Require(result[1] == kSentinel);

    Require(sceSocialScreenDialogOpen(&param) == 0);
    Require(sceSocialScreenDialogGetStatus() == kStatusFinished);
    Require(sceSocialScreenDialogUpdateStatus() == kStatusFinished);

    Require(sceSocialScreenDialogGetResult(nullptr) == kErrArgNull);
    Require(sceSocialScreenDialogGetResult(result) == 0);
    Require(result[0] == kResultUserCanceled);
    Require(result[1] == kSentinel);

    Require(sceSocialScreenDialogClose() == 0);
    Require(sceSocialScreenDialogTerminate() == 0);
    Require(sceSocialScreenDialogTerminate() == kErrNotInitialized);
    Require(sceSocialScreenDialogOpen(nullptr) == kErrNotInitialized);
    Require(sceSocialScreenDialogGetResult(nullptr) == kErrNotInitialized);
    Require(sceSocialScreenDialogGetStatus() == kStatusNone);
    Require(sceSocialScreenDialogUpdateStatus() == kStatusNone);

    Require(sceSocialScreenDialogInitialize() == 0);
    Require(sceSocialScreenDialogUpdateStatus() == kStatusInitialized);
    Require(sceSocialScreenDialogClose() == 0);
    Require(sceSocialScreenDialogUpdateStatus() == kStatusFinished);
    std::memset(result, 0x5a, sizeof(result));
    Require(sceSocialScreenDialogGetResult(result) == 0);
    Require(result[0] == kResultUserCanceled);
    Require(result[1] == kSentinel);
    Require(sceSocialScreenDialogTerminate() == 0);
    Require(sceSocialScreenDialogTerminate() == kErrNotInitialized);
    Require(sceSocialScreenDialogOpen(nullptr) == kErrNotInitialized);
    Require(sceSocialScreenDialogOpen(&param) == kErrNotInitialized);
    Require(sceSocialScreenDialogClose() == kErrNotInitialized);
    Require(sceSocialScreenDialogGetResult(nullptr) == kErrNotInitialized);
    Require(sceSocialScreenDialogGetResult(result) == kErrNotInitialized);
    Require(sceSocialScreenDialogGetStatus() == kStatusNone);
    Require(sceSocialScreenDialogUpdateStatus() == kStatusNone);
}
