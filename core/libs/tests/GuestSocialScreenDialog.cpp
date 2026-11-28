#include "prx/libSceSocialScreenDialog/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
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
constexpr std::int32_t kSentinel = static_cast<std::int32_t>(0x5a5a5a5a);
constexpr int kRaceRounds = 200000;

template <typename Finish>
void RequireTerminateWins(Finish finish) {
    std::atomic<int> round{0};
    std::atomic<int> done{0};
    std::thread terminator([&] {
        for (int i = 1; i <= kRaceRounds; ++i) {
            while (round.load() != i) std::this_thread::yield();
            sceSocialScreenDialogTerminate();
            done.fetch_add(1);
        }
    });
    for (int i = 1; i <= kRaceRounds; ++i) {
        Require(sceSocialScreenDialogInitialize() == 0);
        round.store(i);
        finish();
        while (done.load() != i) std::this_thread::yield();
        Require(sceSocialScreenDialogGetStatus() == kStatusNone);
    }
    terminator.join();
}

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
    Require(result[0] == kSentinel);
    Require(result[1] == kSentinel);
    Require(sceSocialScreenDialogGetStatus() == kStatusNone);
    Require(sceSocialScreenDialogUpdateStatus() == kStatusNone);

    Require(sceSocialScreenDialogInitialize() == 0);
    Require(sceSocialScreenDialogInitialize() == kErrAlreadyInitialized);
    Require(sceSocialScreenDialogGetStatus() == kStatusInitialized);
    Require(sceSocialScreenDialogUpdateStatus() == kStatusInitialized);

    Require(sceSocialScreenDialogOpen(nullptr) == kErrArgNull);
    errno = 71;
    Require(sceSocialScreenDialogOpen(reinterpret_cast<const void*>(0x10)) == kErrArgNull);
    Require(errno == 71);
    std::memset(result, 0x5a, sizeof(result));
    Require(sceSocialScreenDialogGetResult(nullptr) == kErrArgNull);
    Require(sceSocialScreenDialogGetResult(result) == kErrNotFinished);
    Require(result[0] == kSentinel);
    Require(result[1] == kSentinel);

    Require(sceSocialScreenDialogOpen(&param) == 0);
    Require(sceSocialScreenDialogGetStatus() == kStatusFinished);
    Require(sceSocialScreenDialogUpdateStatus() == kStatusFinished);

    Require(sceSocialScreenDialogGetResult(nullptr) == kErrArgNull);
    errno = 71;
    Require(sceSocialScreenDialogGetResult(reinterpret_cast<void*>(0x10)) == kErrArgNull);
    Require(errno == 71);
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
    errno = 71;
    Require(sceSocialScreenDialogGetResult(reinterpret_cast<void*>(0x10)) == kErrArgNull);
    Require(errno == 71);
    Require(sceSocialScreenDialogTerminate() == 0);
    Require(sceSocialScreenDialogTerminate() == kErrNotInitialized);
    Require(sceSocialScreenDialogOpen(nullptr) == kErrNotInitialized);
    Require(sceSocialScreenDialogOpen(&param) == kErrNotInitialized);
    Require(sceSocialScreenDialogClose() == kErrNotInitialized);
    Require(sceSocialScreenDialogGetResult(nullptr) == kErrNotInitialized);
    Require(sceSocialScreenDialogGetResult(result) == kErrNotInitialized);
    Require(sceSocialScreenDialogGetStatus() == kStatusNone);
    Require(sceSocialScreenDialogUpdateStatus() == kStatusNone);

    RequireTerminateWins([&] { sceSocialScreenDialogOpen(&param); });
    RequireTerminateWins([] { sceSocialScreenDialogClose(); });
}
