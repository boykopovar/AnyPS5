#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc PendingTaskSafetyFlag check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

void TestAlive() {
    WebRtcPendingTaskSafetyFlag flag{1, true};
    Require(_ZNK6webrtc21PendingTaskSafetyFlag5aliveEv_nid_postfix(&flag));
    _ZN6webrtc21PendingTaskSafetyFlag11SetNotAliveEv_nid_postfix(&flag);
    Require(!flag.alive);
    Require(flag.refCount == 1);
    Require(!_ZNK6webrtc21PendingTaskSafetyFlag5aliveEv_nid_postfix(&flag));
    _ZN6webrtc21PendingTaskSafetyFlag11SetNotAliveEv_nid_postfix(&flag);
    Require(!_ZNK6webrtc21PendingTaskSafetyFlag5aliveEv_nid_postfix(&flag));
}

void TestNullFlag() {
    RequireInvalidArgument([] { _ZN6webrtc21PendingTaskSafetyFlag11SetNotAliveEv_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZNK6webrtc21PendingTaskSafetyFlag5aliveEv_nid_postfix(nullptr); });
}

}

int main() {
    TestAlive();
    TestNullFlag();
    std::puts("libwebrtc PendingTaskSafetyFlag tests passed");
}
