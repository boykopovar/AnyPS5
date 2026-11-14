#include "prx/libSceNpEulaDialog/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <cerrno>
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
            sceNpEulaDialogTerminate();
            done.fetch_add(1);
        }
    });
    for (int i = 1; i <= kRaceRounds; ++i) {
        Require(sceNpEulaDialogInitialize() == 0);
        round.store(i);
        finish();
        while (done.load() != i) std::this_thread::yield();
        Require(sceNpEulaDialogGetStatus() == kStatusNone);
    }
    terminator.join();
}

}

int main() {
    std::int64_t param = 0;
    std::int32_t result[2] = {-1, -1};

    Require(sceNpEulaDialogTerminate() == kErrNotInitialized);
    Require(sceNpEulaDialogOpen(nullptr) == kErrNotInitialized);
    Require(sceNpEulaDialogOpen(&param) == kErrNotInitialized);
    Require(sceNpEulaDialogClose() == kErrNotInitialized);
    Require(sceNpEulaDialogGetResult(nullptr) == kErrNotInitialized);
    Require(sceNpEulaDialogGetResult(result) == kErrNotInitialized);
    Require(sceNpEulaDialogGetStatus() == kStatusNone);
    Require(sceNpEulaDialogUpdateStatus() == kStatusNone);

    Require(sceNpEulaDialogInitialize() == 0);
    Require(sceNpEulaDialogInitialize() == kErrAlreadyInitialized);
    Require(sceNpEulaDialogGetStatus() == kStatusInitialized);
    Require(sceNpEulaDialogUpdateStatus() == kStatusInitialized);

    Require(sceNpEulaDialogOpen(nullptr) == kErrArgNull);
    Require(sceNpEulaDialogGetResult(result) == kErrNotFinished);

    Require(sceNpEulaDialogOpen(&param) == 0);
    Require(sceNpEulaDialogGetStatus() == kStatusFinished);
    Require(sceNpEulaDialogUpdateStatus() == kStatusFinished);

    Require(sceNpEulaDialogGetResult(nullptr) == kErrArgNull);
    errno = 71;
    Require(sceNpEulaDialogGetResult(reinterpret_cast<void*>(0x10)) == kErrArgNull);
    Require(errno == 71);
    Require(sceNpEulaDialogGetResult(result) == 0);
    Require(result[0] == kResultUserCanceled);
    Require(result[1] == -1);

    Require(sceNpEulaDialogClose() == 0);
    Require(sceNpEulaDialogTerminate() == 0);
    Require(sceNpEulaDialogTerminate() == kErrNotInitialized);
    Require(sceNpEulaDialogOpen(nullptr) == kErrNotInitialized);
    Require(sceNpEulaDialogGetResult(nullptr) == kErrNotInitialized);
    Require(sceNpEulaDialogGetStatus() == kStatusNone);
    Require(sceNpEulaDialogUpdateStatus() == kStatusNone);

    Require(sceNpEulaDialogInitialize() == 0);
    Require(sceNpEulaDialogUpdateStatus() == kStatusInitialized);
    Require(sceNpEulaDialogClose() == 0);
    Require(sceNpEulaDialogUpdateStatus() == kStatusFinished);
    result[0] = -1;
    Require(sceNpEulaDialogGetResult(result) == 0);
    Require(result[0] == kResultUserCanceled);
    errno = 71;
    Require(sceNpEulaDialogGetResult(reinterpret_cast<void*>(0x10)) == kErrArgNull);
    Require(errno == 71);
    Require(sceNpEulaDialogTerminate() == 0);
    Require(sceNpEulaDialogTerminate() == kErrNotInitialized);
    Require(sceNpEulaDialogOpen(nullptr) == kErrNotInitialized);
    Require(sceNpEulaDialogOpen(&param) == kErrNotInitialized);
    Require(sceNpEulaDialogClose() == kErrNotInitialized);
    Require(sceNpEulaDialogGetResult(nullptr) == kErrNotInitialized);
    Require(sceNpEulaDialogGetResult(result) == kErrNotInitialized);
    Require(sceNpEulaDialogGetStatus() == kStatusNone);
    Require(sceNpEulaDialogUpdateStatus() == kStatusNone);

    RequireTerminateWins([&] { sceNpEulaDialogOpen(&param); });
    RequireTerminateWins([] { sceNpEulaDialogClose(); });
}
