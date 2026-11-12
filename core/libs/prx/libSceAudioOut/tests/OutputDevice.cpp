#include "prx/libSceAudioOut/include/OutputDevice.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* reason) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", reason);
        std::exit(1);
    }
}

}

int main() {
    const std::vector<std::string> devices{"Built-in Audio", "USB Headset"};
    require(FindOutputDevice(devices, "USB Headset") == std::optional<std::size_t>{1}, "a listed device is found by its exact name");
    require(FindOutputDevice(devices, "usb headset") == std::nullopt, "device names are matched exactly, not by case");
    require(FindOutputDevice(devices, "Missing") == std::nullopt, "an unlisted device is not found");
    require(FindOutputDevice({}, "Built-in Audio") == std::nullopt, "an empty device list finds nothing");
    std::puts("audio output device selection passed");
    return 0;
}
