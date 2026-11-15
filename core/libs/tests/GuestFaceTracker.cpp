#include "prx/libSceFaceTracker/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
}

constexpr int kErrInvalid = static_cast<int>(0x812a0001);
constexpr int kErrNullContext = static_cast<int>(0x812a0002);
constexpr int kErrNotInitialized = static_cast<int>(0x812a0003);

constexpr std::size_t kContextSize = 0x2000;
constexpr std::size_t kUserDb = 0x1598;
constexpr std::size_t kUserDbEntries = 0x28;
constexpr std::size_t kUserDbFeatures = 0x30;
constexpr std::size_t kUserDbCount = 0x48;
constexpr std::size_t kTrackCount = 0x1778;
constexpr std::size_t kTrackBase = 0x1780;
constexpr std::size_t kInitFlag = 0x180c;

template <typename T>
void StoreAt(std::vector<std::uint8_t>& buf, std::size_t offset, const T& value) {
    std::memcpy(buf.data() + offset, &value, sizeof(value));
}

}

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
    void* feature = nullptr;
    int featureSize = 0;
    Require(sceFaceTrackerGetUserFeature(1, &feature, &featureSize, nullptr) == kErrNullContext);

    std::vector<std::uint8_t> context(kContextSize, 0);
    Require(sceFaceTrackerGetUserFeature(1, &feature, &featureSize, context.data()) == kErrNotInitialized);

    context[kInitFlag] = 1;
    Require(sceFaceTrackerGetUserFeature(0, &feature, &featureSize, context.data()) == kErrInvalid);
    Require(sceFaceTrackerGetUserFeature(-1, &feature, &featureSize, context.data()) == kErrInvalid);
    Require(sceFaceTrackerGetUserFeature(1, nullptr, &featureSize, context.data()) == kErrInvalid);
    Require(sceFaceTrackerGetUserFeature(1, &feature, nullptr, context.data()) == kErrInvalid);

    errno = 71;
    Require(sceFaceTrackerGetUserFeature(1, reinterpret_cast<void**>(0x10), &featureSize, context.data()) == kErrInvalid);
    Require(errno == 71);

    errno = 71;
    Require(sceFaceTrackerGetUserFeature(1, &feature, reinterpret_cast<int*>(0x10), context.data()) == kErrInvalid);
    Require(errno == 71);

    errno = 71;
    Require(sceFaceTrackerGetDefaultConfig(reinterpret_cast<void*>(0x10)) == kErrInvalid);
    Require(errno == 71);

    std::vector<std::uint8_t> config(0x80, 0);
    Require(sceFaceTrackerGetDefaultConfig(config.data()) == 0);

    errno = 71;
    Require(sceFaceTrackerRegisterUser(1, reinterpret_cast<const void*>(0x10), 16, context.data()) == kErrInvalid);
    Require(errno == 71);

    void* badContext = reinterpret_cast<void*>(0x10);
    errno = 71;
    Require(sceFaceTrackerRegisterUser(1, config.data(), 16, badContext) == kErrInvalid && errno == 71);
    Require(sceFaceTrackerUnregisterUser(1, badContext) == kErrInvalid && errno == 71);
    Require(sceFaceTrackerGetUserFeature(1, &feature, &featureSize, badContext) == kErrInvalid && errno == 71);
    Require(sceFaceTrackerGetUserIdByTrackId(0, badContext) == kErrInvalid && errno == 71);
    Require(sceFaceTrackerGetTrackIdByUserId(1, badContext) == kErrInvalid && errno == 71);
    Require(sceFaceTrackerRegisterStartTrackingCallback(nullptr, nullptr, badContext) == kErrInvalid && errno == 71);

    std::vector<std::uint8_t> dbEntries(2 * 0xc, 0);
    std::vector<std::uint8_t> dbFeatures(2 * 0x4100, 0);
    std::vector<std::uint8_t> trackRecords(0x1340, 0);
    auto* const badGuestPtr = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x10));

    StoreAt(context, kUserDb + kUserDbCount, std::int32_t{1});
    StoreAt(context, kUserDb + kUserDbEntries, badGuestPtr);
    StoreAt(context, kUserDb + kUserDbFeatures, static_cast<void*>(dbFeatures.data()));
    errno = 71;
    Require(sceFaceTrackerRegisterUser(1, config.data(), 16, context.data()) == kErrInvalid && errno == 71);
    errno = 71;
    Require(sceFaceTrackerUnregisterUser(1, context.data()) == kErrInvalid && errno == 71);
    errno = 71;
    Require(sceFaceTrackerGetUserFeature(1, &feature, &featureSize, context.data()) == kErrInvalid && errno == 71);

    StoreAt(context, kUserDb + kUserDbEntries, static_cast<void*>(dbEntries.data()));
    StoreAt(context, kUserDb + kUserDbFeatures, badGuestPtr);
    errno = 71;
    Require(sceFaceTrackerRegisterUser(1, config.data(), 16, context.data()) == kErrInvalid && errno == 71);

    StoreAt(context, kTrackCount, std::int32_t{1});
    StoreAt(context, kTrackBase, badGuestPtr);
    errno = 71;
    Require(sceFaceTrackerGetUserIdByTrackId(1, context.data()) == kErrInvalid && errno == 71);
    errno = 71;
    Require(sceFaceTrackerGetTrackIdByUserId(1, context.data()) == kErrInvalid && errno == 71);

    StoreAt(context, kUserDb + kUserDbFeatures, static_cast<void*>(dbFeatures.data()));
    StoreAt(context, kTrackBase, static_cast<void*>(trackRecords.data()));
    StoreAt(trackRecords, 0x18, std::int32_t{7});
    StoreAt(trackRecords, 0x24, std::int32_t{1});
    errno = 71;
    Require(sceFaceTrackerGetUserIdByTrackId(7, context.data()) == 1 && errno == 71);
    errno = 71;
    Require(sceFaceTrackerGetTrackIdByUserId(1, context.data()) == 7 && errno == 71);
    errno = 71;
    Require(sceFaceTrackerRegisterUser(1, config.data(), 16, context.data()) == 0 && errno == 71);
    errno = 71;
    Require(sceFaceTrackerGetUserFeature(1, &feature, &featureSize, context.data()) == 0 && errno == 71);
    Require(feature == dbFeatures.data() && featureSize == 0x4100);
    errno = 71;
    Require(sceFaceTrackerUnregisterUser(1, context.data()) == 0 && errno == 71);
    errno = 71;
    Require(sceFaceTrackerGetTrackIdByUserId(1, context.data()) == -1 && errno == 71);
    errno = 71;
    Require(sceFaceTrackerRegisterStartTrackingCallback(nullptr, nullptr, context.data()) == 0 && errno == 71);

    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
    return 0;
}
