#include "prx/libSceNpSnsFacebookDialog/include/Api.hpp"
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
            while (round.load() != i) {}
            sceNpSnsFacebookDialogTerminate();
            done.fetch_add(1);
        }
    });
    for (int i = 1; i <= kRaceRounds; ++i) {
        Require(sceNpSnsFacebookDialogInitialize() == 0);
        round.store(i);
        finish();
        while (done.load() != i) {}
        Require(sceNpSnsFacebookDialogGetStatus() == kStatusNone);
    }
    terminator.join();
}

}

int main() {
    std::int64_t param = 0;
    std::int32_t result[2] = {-1, -1};

    Require(sceNpSnsFacebookDialogTerminate() == kErrNotInitialized);
    Require(sceNpSnsFacebookDialogOpen(nullptr) == kErrNotInitialized);
    Require(sceNpSnsFacebookDialogOpen(&param) == kErrNotInitialized);
    Require(sceNpSnsFacebookDialogClose() == kErrNotInitialized);
    Require(sceNpSnsFacebookDialogGetResult(nullptr) == kErrNotInitialized);
    Require(sceNpSnsFacebookDialogGetResult(result) == kErrNotInitialized);
    Require(sceNpSnsFacebookDialogGetStatus() == kStatusNone);
    Require(sceNpSnsFacebookDialogUpdateStatus() == kStatusNone);

    Require(sceNpSnsFacebookDialogInitialize() == 0);
    Require(sceNpSnsFacebookDialogInitialize() == kErrAlreadyInitialized);
    Require(sceNpSnsFacebookDialogGetStatus() == kStatusInitialized);
    Require(sceNpSnsFacebookDialogUpdateStatus() == kStatusInitialized);

    Require(sceNpSnsFacebookDialogOpen(nullptr) == kErrArgNull);
    Require(sceNpSnsFacebookDialogGetResult(result) == kErrNotFinished);

    Require(sceNpSnsFacebookDialogOpen(&param) == 0);
    Require(sceNpSnsFacebookDialogGetStatus() == kStatusFinished);
    Require(sceNpSnsFacebookDialogUpdateStatus() == kStatusFinished);

    Require(sceNpSnsFacebookDialogGetResult(nullptr) == kErrArgNull);
    Require(sceNpSnsFacebookDialogGetResult(result) == 0);
    Require(result[0] == kResultUserCanceled);
    Require(result[1] == -1);

    Require(sceNpSnsFacebookDialogClose() == 0);
    Require(sceNpSnsFacebookDialogTerminate() == 0);
    Require(sceNpSnsFacebookDialogTerminate() == kErrNotInitialized);
    Require(sceNpSnsFacebookDialogOpen(nullptr) == kErrNotInitialized);
    Require(sceNpSnsFacebookDialogOpen(&param) == kErrNotInitialized);
    Require(sceNpSnsFacebookDialogClose() == kErrNotInitialized);
    Require(sceNpSnsFacebookDialogGetResult(nullptr) == kErrNotInitialized);
    Require(sceNpSnsFacebookDialogGetResult(result) == kErrNotInitialized);
    Require(sceNpSnsFacebookDialogGetStatus() == kStatusNone);
    Require(sceNpSnsFacebookDialogUpdateStatus() == kStatusNone);

    Require(sceNpSnsFacebookDialogInitialize() == 0);
    Require(sceNpSnsFacebookDialogUpdateStatus() == kStatusInitialized);
    Require(sceNpSnsFacebookDialogClose() == 0);
    Require(sceNpSnsFacebookDialogUpdateStatus() == kStatusFinished);
    result[0] = -1;
    Require(sceNpSnsFacebookDialogGetResult(result) == 0);
    Require(result[0] == kResultUserCanceled);
    Require(sceNpSnsFacebookDialogTerminate() == 0);
    Require(sceNpSnsFacebookDialogTerminate() == kErrNotInitialized);
    Require(sceNpSnsFacebookDialogOpen(nullptr) == kErrNotInitialized);
    Require(sceNpSnsFacebookDialogOpen(&param) == kErrNotInitialized);
    Require(sceNpSnsFacebookDialogClose() == kErrNotInitialized);
    Require(sceNpSnsFacebookDialogGetResult(nullptr) == kErrNotInitialized);
    Require(sceNpSnsFacebookDialogGetResult(result) == kErrNotInitialized);
    Require(sceNpSnsFacebookDialogGetStatus() == kStatusNone);
    Require(sceNpSnsFacebookDialogUpdateStatus() == kStatusNone);

    RequireTerminateWins([&] { sceNpSnsFacebookDialogOpen(&param); });
    RequireTerminateWins([] { sceNpSnsFacebookDialogClose(); });
}
