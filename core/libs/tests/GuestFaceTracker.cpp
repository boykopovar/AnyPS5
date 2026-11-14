#include "prx/libSceFaceTracker/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdlib>
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

constexpr std::size_t kContextSize = 0x2000;
constexpr std::size_t kInitFlag = 0x180c;

}

int main() {
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

    return 0;
}
