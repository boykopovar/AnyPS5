#include "prx/libSceAudioOut/include/OutputDevice.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

int main() {
    const std::vector<std::string> devices{"Built-in Audio", "USB Headset"};
    Require(FindOutputDevice(devices, "USB Headset") == std::optional<std::size_t>{1}, "a listed device is found by its exact name");
    Require(FindOutputDevice(devices, "usb headset") == std::nullopt, "device names are matched exactly, not by case");
    Require(FindOutputDevice(devices, "Missing") == std::nullopt, "an unlisted device is not found");
    Require(FindOutputDevice({}, "Built-in Audio") == std::nullopt, "an empty device list finds nothing");
    std::puts("audio output device selection passed");
    return 0;
}
