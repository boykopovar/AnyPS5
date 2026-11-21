#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc AudioDeviceGeneric check failed at line %u\n", location.line());
    std::abort();
}

unsigned char storage[64];

WebRtcAudioDeviceGeneric* Device() {
    std::memset(storage, 0xC3, sizeof(storage));
    return reinterpret_cast<WebRtcAudioDeviceGeneric*>(storage);
}

bool DeviceUnchanged() {
    for (unsigned char byte : storage)
        if (byte != 0xC3) return false;
    return true;
}

void TestAvailability() {
    WebRtcAudioDeviceGeneric* device = Device();
    Require(!_ZNK6webrtc18AudioDeviceGeneric21BuiltInAECIsAvailableEv_nid_postfix(device));
    Require(!_ZNK6webrtc18AudioDeviceGeneric21BuiltInAGCIsAvailableEv_nid_postfix(device));
    Require(!_ZNK6webrtc18AudioDeviceGeneric20BuiltInNSIsAvailableEv_nid_postfix(device));
    Require(DeviceUnchanged());
}

void TestEnable() {
    WebRtcAudioDeviceGeneric* device = Device();
    for (bool enable : {true, false}) {
        Require(_ZN6webrtc18AudioDeviceGeneric16EnableBuiltInAECEb_nid_postfix(device, enable) == -1);
        Require(_ZN6webrtc18AudioDeviceGeneric16EnableBuiltInAGCEb_nid_postfix(device, enable) == -1);
        Require(_ZN6webrtc18AudioDeviceGeneric15EnableBuiltInNSEb_nid_postfix(device, enable) == -1);
    }
    Require(!_ZNK6webrtc18AudioDeviceGeneric21BuiltInAECIsAvailableEv_nid_postfix(device));
    Require(DeviceUnchanged());
}

void TestUnderrunCount() {
    WebRtcAudioDeviceGeneric* device = Device();
    Require(_ZNK6webrtc18AudioDeviceGeneric23GetPlayoutUnderrunCountEv_nid_postfix(device) == -1);
    Require(DeviceUnchanged());
}

}

int main() {
    TestAvailability();
    TestEnable();
    TestUnderrunCount();
    return 0;
}
