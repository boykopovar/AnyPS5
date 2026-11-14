#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc AudioNetworkAdaptorImpl::Config check failed at line %u\n", location.line());
    std::abort();
}

void TestConstructorClearsTheEventLog() {
    WebRtcAudioNetworkAdaptorConfig config;
    std::memset(&config, 0xA5, sizeof(config));
    _ZN6webrtc23AudioNetworkAdaptorImpl6ConfigC1Ev_nid_postfix(&config);
    Require(config.eventLog == nullptr);
}

void TestDestructorLeavesTheEventLog() {
    int eventLog = 0;
    WebRtcAudioNetworkAdaptorConfig config {&eventLog};
    _ZN6webrtc23AudioNetworkAdaptorImpl6ConfigD1Ev_nid_postfix(&config);
    Require(config.eventLog == &eventLog);
    _ZN6webrtc23AudioNetworkAdaptorImpl6ConfigC1Ev_nid_postfix(&config);
    _ZN6webrtc23AudioNetworkAdaptorImpl6ConfigD1Ev_nid_postfix(&config);
    Require(config.eventLog == nullptr);
}

void TestNullConfigThrows() {
    try {
        _ZN6webrtc23AudioNetworkAdaptorImpl6ConfigC1Ev_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

}

int main() {
    TestConstructorClearsTheEventLog();
    TestDestructorLeavesTheEventLog();
    TestNullConfigThrows();
    return 0;
}
