#include "prx/libSceHmdSetupDialog/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <thread>

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
constexpr int kRaceRounds = 200000;

template <typename Finish>
void RequireTerminateWins(Finish finish) {
    std::atomic<int> round{0};
    std::atomic<int> done{0};
    std::thread terminator([&] {
        for (int i = 1; i <= kRaceRounds; ++i) {
            while (round.load() != i) std::this_thread::yield();
            sceHmdSetupDialogTerminate();
            done.fetch_add(1);
        }
    });
    for (int i = 1; i <= kRaceRounds; ++i) {
        Require(sceHmdSetupDialogInitialize() == 0);
        round.store(i);
        finish();
        while (done.load() != i) std::this_thread::yield();
        Require(sceHmdSetupDialogGetStatus() == kStatusNone);
    }
    terminator.join();
}

}

int main() {
    std::int64_t param = 0;
    std::int32_t result[2] = {-1, -1};

    Require(sceHmdSetupDialogTerminate() == kErrNotInitialized);
    Require(sceHmdSetupDialogOpen(nullptr) == kErrNotInitialized);
    Require(sceHmdSetupDialogOpen(&param) == kErrNotInitialized);
    Require(sceHmdSetupDialogClose() == kErrNotInitialized);
    Require(sceHmdSetupDialogGetResult(nullptr) == kErrNotInitialized);
    Require(sceHmdSetupDialogGetResult(result) == kErrNotInitialized);
    Require(sceHmdSetupDialogGetStatus() == kStatusNone);
    Require(sceHmdSetupDialogUpdateStatus() == kStatusNone);

    Require(sceHmdSetupDialogInitialize() == 0);
    Require(sceHmdSetupDialogInitialize() == kErrAlreadyInitialized);
    Require(sceHmdSetupDialogGetStatus() == kStatusInitialized);
    Require(sceHmdSetupDialogUpdateStatus() == kStatusInitialized);

    Require(sceHmdSetupDialogOpen(nullptr) == kErrArgNull);
    Require(sceHmdSetupDialogGetResult(result) == kErrNotFinished);

    Require(sceHmdSetupDialogOpen(&param) == 0);
    Require(sceHmdSetupDialogGetStatus() == kStatusFinished);
    Require(sceHmdSetupDialogUpdateStatus() == kStatusFinished);

    Require(sceHmdSetupDialogGetResult(nullptr) == kErrArgNull);
    Require(sceHmdSetupDialogGetResult(result) == 0);
    Require(result[0] == kResultUserCanceled);
    Require(result[1] == -1);

    Require(sceHmdSetupDialogClose() == 0);
    Require(sceHmdSetupDialogTerminate() == 0);
    Require(sceHmdSetupDialogTerminate() == kErrNotInitialized);
    Require(sceHmdSetupDialogOpen(nullptr) == kErrNotInitialized);
    Require(sceHmdSetupDialogGetResult(nullptr) == kErrNotInitialized);
    Require(sceHmdSetupDialogGetStatus() == kStatusNone);
    Require(sceHmdSetupDialogUpdateStatus() == kStatusNone);

    Require(sceHmdSetupDialogInitialize() == 0);
    Require(sceHmdSetupDialogUpdateStatus() == kStatusInitialized);
    Require(sceHmdSetupDialogClose() == 0);
    Require(sceHmdSetupDialogUpdateStatus() == kStatusFinished);
    result[0] = -1;
    Require(sceHmdSetupDialogGetResult(result) == 0);
    Require(result[0] == kResultUserCanceled);
    Require(sceHmdSetupDialogTerminate() == 0);
    Require(sceHmdSetupDialogTerminate() == kErrNotInitialized);
    Require(sceHmdSetupDialogOpen(nullptr) == kErrNotInitialized);
    Require(sceHmdSetupDialogOpen(&param) == kErrNotInitialized);
    Require(sceHmdSetupDialogClose() == kErrNotInitialized);
    Require(sceHmdSetupDialogGetResult(nullptr) == kErrNotInitialized);
    Require(sceHmdSetupDialogGetResult(result) == kErrNotInitialized);
    Require(sceHmdSetupDialogGetStatus() == kStatusNone);
    Require(sceHmdSetupDialogUpdateStatus() == kStatusNone);

    RequireTerminateWins([&] { sceHmdSetupDialogOpen(&param); });
    RequireTerminateWins([] { sceHmdSetupDialogClose(); });
}
