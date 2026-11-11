#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc AudioState::Config check failed at line %u\n", location.line());
    std::abort();
}

void TestConstructorClearsEveryReference() {
    WebRtcAudioStateConfig config;
    std::memset(&config, 0xA5, sizeof(config));
    _ZN6webrtc10AudioState6ConfigC1Ev_nid_postfix(&config);
    Require(config.audioMixer == nullptr);
    Require(config.audioProcessing == nullptr);
    Require(config.audioDeviceModule == nullptr);
    Require(config.asyncAudioProcessingFactory == nullptr);
}

void TestNullConfigThrows() {
    try {
        _ZN6webrtc10AudioState6ConfigC1Ev_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

}

struct FakeReference {
    const void* const* vtable;
    char name;
};

char calls[16];
int callCount;

void APS5_VABI FakeAddRef(const FakeReference* object) {
    calls[callCount++] = static_cast<char>(object->name - 'a' + 'A');
}

int APS5_VABI FakeRelease(const FakeReference* object) {
    calls[callCount++] = object->name;
    return 1;
}

const void* const FakeVtable[] = {
    reinterpret_cast<const void*>(&FakeAddRef),
    reinterpret_cast<const void*>(&FakeRelease),
};

void TestDestructorReleasesInReverseOrder() {
    FakeReference mixer{FakeVtable, 'm'};
    FakeReference processing{FakeVtable, 'p'};
    FakeReference device{FakeVtable, 'd'};
    FakeReference factory{FakeVtable, 'f'};
    WebRtcAudioStateConfig config{&mixer, &processing, &device, &factory};
    callCount = 0;
    _ZN6webrtc10AudioState6ConfigD1Ev_nid_postfix(&config);
    Require(callCount == 4);
    Require(std::memcmp(calls, "fdpm", 4) == 0);
}

void TestDestructorSkipsNullReferences() {
    FakeReference processing{FakeVtable, 'p'};
    WebRtcAudioStateConfig config{};
    _ZN6webrtc10AudioState6ConfigC1Ev_nid_postfix(&config);
    callCount = 0;
    _ZN6webrtc10AudioState6ConfigD1Ev_nid_postfix(&config);
    Require(callCount == 0);
    config.audioProcessing = &processing;
    _ZN6webrtc10AudioState6ConfigD1Ev_nid_postfix(&config);
    Require(callCount == 1 && calls[0] == 'p');
}

int main() {
    TestConstructorClearsEveryReference();
    TestNullConfigThrows();
    TestDestructorReleasesInReverseOrder();
    TestDestructorSkipsNullReferences();
    return 0;
}
