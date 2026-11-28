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

constexpr int kErrNullContext = static_cast<int>(0x812a0002);
constexpr int kErrNotInitialized = static_cast<int>(0x812a0003);

constexpr std::size_t kContextSize = 0x2000;
constexpr std::size_t kInitFlag = 0x180c;

using RegisterCallback = decltype(&sceFaceTrackerRegisterStartTrackingCallback);

const RegisterCallback kRegistrations[5] = {
    sceFaceTrackerRegisterStartTrackingCallback,
    sceFaceTrackerRegisterFixUserIdCallback,
    sceFaceTrackerRegisterStopTrackingCallback,
    sceFaceTrackerRegisterLostTargetCallback,
    sceFaceTrackerRegisterRecoveryFromLostTargetCallback,
};

constexpr std::size_t kCallbackOffsets[5] = {0x1810, 0x1818, 0x1820, 0x1828, 0x1830};
constexpr std::size_t kArgumentOffsets[5] = {0x1838, 0x1840, 0x1848, 0x1850, 0x1858};

void* StoredPtr(const std::vector<std::uint8_t>& context, std::size_t off) {
    void* p;
    std::memcpy(&p, context.data() + off, sizeof(p));
    return p;
}

}

int main() {
    std::vector<std::uint8_t> markers(64, 0);
    void* callbacks[5];
    void* arguments[5];
    for (int i = 0; i < 5; ++i) {
        callbacks[i] = markers.data() + static_cast<std::size_t>(i) * 8;
        arguments[i] = markers.data() + 32 + static_cast<std::size_t>(i) * 8;
    }

    for (int i = 0; i < 5; ++i) {
        Require(kRegistrations[i](callbacks[i], arguments[i], nullptr) == kErrNullContext);
    }

    std::vector<std::uint8_t> context(kContextSize, 0);
    for (int i = 0; i < 5; ++i) {
        Require(kRegistrations[i](callbacks[i], arguments[i], context.data()) == kErrNotInitialized);
    }

    context[kInitFlag] = 1;
    for (int i = 0; i < 5; ++i) {
        Require(kRegistrations[i](callbacks[i], arguments[i], context.data()) == 0);
        Require(StoredPtr(context, kCallbackOffsets[i]) == callbacks[i]);
        Require(StoredPtr(context, kArgumentOffsets[i]) == arguments[i]);
    }

    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5; ++j) {
            if (i == j) {
                continue;
            }
            Require(StoredPtr(context, kCallbackOffsets[j]) == callbacks[j]);
            Require(StoredPtr(context, kArgumentOffsets[j]) == arguments[j]);
        }
    }

    void* replacement = markers.data() + 56;
    void* replacementArg = markers.data() + 48;
    Require(sceFaceTrackerRegisterStartTrackingCallback(replacement, replacementArg, context.data()) == 0);
    Require(StoredPtr(context, 0x1810) == replacement);
    Require(StoredPtr(context, 0x1838) == replacementArg);
    Require(StoredPtr(context, 0x1818) == callbacks[1]);
    Require(StoredPtr(context, 0x1858) == arguments[4]);
    return 0;
}
