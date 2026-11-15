#include "prx/libSceFaceTracker/include/Api.hpp"
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

constexpr int kErrInvalid = static_cast<int>(0x812a0001);
constexpr int kErrNullContext = static_cast<int>(0x812a0002);
constexpr int kErrNotInitialized = static_cast<int>(0x812a0003);
constexpr int kErrFull = static_cast<int>(0x812a0005);
constexpr int kErrNotFound = static_cast<int>(0x812a0006);

constexpr std::size_t kContextSize = 0x2000;
constexpr std::size_t kUserDb = 0x1598;
constexpr std::size_t kInitFlag = 0x180c;
constexpr std::size_t kUserDbEntries = 0x28;
constexpr std::size_t kUserDbFeatures = 0x30;
constexpr std::size_t kUserDbCount = 0x48;
constexpr std::size_t kUserEntryStride = 0xc;
constexpr std::size_t kFeatureStride = 0x4100;
constexpr std::size_t kFeatureElementStride = 0x410;
constexpr int kFeatureElementCount = 0x10;

std::int32_t ReadI32(const std::uint8_t* p) {
    std::int32_t v;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

void WriteI32(std::uint8_t* p, std::int32_t v) {
    std::memcpy(p, &v, sizeof(v));
}

void WritePtr(std::uint8_t* p, void* v) {
    std::memcpy(p, &v, sizeof(v));
}

std::uint64_t ReadU64(const std::uint8_t* p) {
    std::uint64_t v;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

}

int main() {
    std::vector<std::uint8_t> feature(16, 0x5A);
    Require(sceFaceTrackerRegisterUser(1, feature.data(), 16, nullptr) == kErrNullContext);
    Require(sceFaceTrackerUnregisterUser(1, nullptr) == kErrNullContext);

    std::vector<std::uint8_t> context(kContextSize, 0);
    Require(sceFaceTrackerRegisterUser(1, feature.data(), 16, context.data()) == kErrNotInitialized);
    Require(sceFaceTrackerUnregisterUser(1, context.data()) == kErrNotInitialized);

    context[kInitFlag] = 1;
    Require(sceFaceTrackerRegisterUser(0, feature.data(), 16, context.data()) == kErrInvalid);
    Require(sceFaceTrackerRegisterUser(-3, feature.data(), 16, context.data()) == kErrInvalid);
    Require(sceFaceTrackerRegisterUser(1, nullptr, 16, context.data()) == kErrInvalid);
    Require(sceFaceTrackerRegisterUser(1, feature.data(), 0, context.data()) == kErrInvalid);
    Require(sceFaceTrackerRegisterUser(1, feature.data(), -1, context.data()) == kErrInvalid);
    Require(sceFaceTrackerRegisterUser(1, feature.data(), 0x4101, context.data()) == kErrInvalid);
    Require(sceFaceTrackerUnregisterUser(0, context.data()) == kErrInvalid);
    Require(sceFaceTrackerRegisterUser(1, feature.data(), 16, context.data()) == kErrFull);
    Require(sceFaceTrackerUnregisterUser(1, context.data()) == kErrNotFound);

    std::vector<std::uint8_t> entries(2 * kUserEntryStride, 0);
    std::vector<std::uint8_t> features(2 * kFeatureStride, 0x3C);
    WritePtr(context.data() + kUserDb + kUserDbEntries, entries.data());
    WritePtr(context.data() + kUserDb + kUserDbFeatures, features.data());
    WriteI32(context.data() + kUserDb + kUserDbCount, 2);

    Require(sceFaceTrackerRegisterUser(7, feature.data(), 16, context.data()) == 0);
    Require(entries[0] == 1 && ReadI32(entries.data() + 8) == 7);
    Require(std::memcmp(features.data(), feature.data(), 16) == 0);

    Require(sceFaceTrackerRegisterUser(7, feature.data(), 16, context.data()) == kErrInvalid);

    std::vector<std::uint8_t> second(4, 0x11);
    Require(sceFaceTrackerRegisterUser(9, second.data(), 4, context.data()) == 0);
    Require(entries[kUserEntryStride] == 1 && ReadI32(entries.data() + kUserEntryStride + 8) == 9);
    Require(std::memcmp(features.data() + kFeatureStride, second.data(), 4) == 0);

    Require(sceFaceTrackerRegisterUser(11, feature.data(), 16, context.data()) == kErrFull);

    Require(sceFaceTrackerUnregisterUser(7, context.data()) == 0);
    Require(entries[0] == 0 && ReadI32(entries.data() + 4) == -1 && ReadI32(entries.data() + 8) == -1);

    bool wiped = true;
    for (int i = 0; i < kFeatureElementCount; ++i) {
        const std::uint8_t* element = features.data() + static_cast<std::size_t>(i) * kFeatureElementStride;
        for (std::size_t b = 0; b < 0x400 && wiped; ++b) {
            wiped = element[b] == 0;
        }
        wiped = wiped && ReadU64(element + 0x400) == 0x1ffffffffULL && ReadU64(element + 0x408) == 0;
    }
    Require(wiped);

    Require(sceFaceTrackerUnregisterUser(7, context.data()) == kErrNotFound);
    Require(sceFaceTrackerRegisterUser(7, feature.data(), 16, context.data()) == 0);
    return 0;
}
