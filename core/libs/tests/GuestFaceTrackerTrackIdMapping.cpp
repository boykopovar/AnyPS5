#include "prx/libSceFaceTracker/include/Api.hpp"
#include "Testing.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

constexpr int kErrInvalid = static_cast<int>(0x812a0001);
constexpr int kErrNullContext = static_cast<int>(0x812a0002);
constexpr int kErrNotInitialized = static_cast<int>(0x812a0003);
constexpr int kErrNotFound = static_cast<int>(0x812a0006);

constexpr std::size_t kContextSize = 0x2000;
constexpr std::size_t kTrackCount = 0x1778;
constexpr std::size_t kTrackBase = 0x1780;
constexpr std::size_t kInitFlag = 0x180c;
constexpr std::size_t kRecordStride = 0x1340;
constexpr std::size_t kRecordTrackId = 0x18;
constexpr std::size_t kRecordUserId = 0x24;

void WriteI32(std::uint8_t* p, std::int32_t v) {
    std::memcpy(p, &v, sizeof(v));
}

void WritePtr(std::uint8_t* p, void* v) {
    std::memcpy(p, &v, sizeof(v));
}

}

int main() {
    Require(sceFaceTrackerGetUserIdByTrackId(1, nullptr) == kErrNullContext);
    Require(sceFaceTrackerGetTrackIdByUserId(1, nullptr) == kErrNullContext);

    std::vector<std::uint8_t> context(kContextSize, 0);
    Require(sceFaceTrackerGetUserIdByTrackId(1, context.data()) == kErrNotInitialized);
    Require(sceFaceTrackerGetTrackIdByUserId(1, context.data()) == kErrNotInitialized);

    context[kInitFlag] = 1;
    Require(sceFaceTrackerGetUserIdByTrackId(-1, context.data()) == kErrInvalid);
    Require(sceFaceTrackerGetTrackIdByUserId(0, context.data()) == kErrInvalid);
    Require(sceFaceTrackerGetTrackIdByUserId(-2, context.data()) == kErrInvalid);
    Require(sceFaceTrackerGetUserIdByTrackId(1, context.data()) == kErrNotFound);
    Require(sceFaceTrackerGetTrackIdByUserId(1, context.data()) == kErrNotFound);

    std::vector<std::uint8_t> records(2 * kRecordStride, 0);
    WritePtr(context.data() + kTrackBase, records.data());
    WriteI32(context.data() + kTrackCount, 2);
    WriteI32(records.data() + kRecordTrackId, 3);
    WriteI32(records.data() + kRecordUserId, 42);
    WriteI32(records.data() + kRecordStride + kRecordTrackId, 9);
    WriteI32(records.data() + kRecordStride + kRecordUserId, -1);

    Require(sceFaceTrackerGetUserIdByTrackId(3, context.data()) == 42);
    Require(sceFaceTrackerGetTrackIdByUserId(42, context.data()) == 3);
    Require(sceFaceTrackerGetUserIdByTrackId(9, context.data()) == -1);
    Require(sceFaceTrackerGetUserIdByTrackId(0, context.data()) == kErrNotFound);
    Require(sceFaceTrackerGetUserIdByTrackId(4, context.data()) == kErrNotFound);
    Require(sceFaceTrackerGetTrackIdByUserId(5, context.data()) == kErrNotFound);

    WriteI32(context.data() + kTrackCount, 0);
    Require(sceFaceTrackerGetUserIdByTrackId(3, context.data()) == kErrNotFound);
    Require(sceFaceTrackerGetTrackIdByUserId(42, context.data()) == kErrNotFound);
    return 0;
}
