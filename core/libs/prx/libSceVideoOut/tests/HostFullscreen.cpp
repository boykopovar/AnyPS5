#include "prx/libSceVideoOut/include/HostFullscreen.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

static void Require(bool value, const char* reason) {
    if (!value) {
        std::fprintf(stderr, "%s\n", reason);
        std::abort();
    }
}

static void ParseTest() {
    Require(ParseHostFullscreenMode(nullptr) == HostFullscreenMode::Windowed, "unset did not stay windowed");
    Require(ParseHostFullscreenMode("") == HostFullscreenMode::Windowed, "empty did not stay windowed");
    Require(ParseHostFullscreenMode("0") == HostFullscreenMode::Windowed, "0 was not windowed");
    Require(ParseHostFullscreenMode("windowed") == HostFullscreenMode::Windowed, "windowed was not windowed");
    Require(ParseHostFullscreenMode("1") == HostFullscreenMode::Borderless, "1 was not borderless");
    Require(ParseHostFullscreenMode("borderless") == HostFullscreenMode::Borderless, "borderless was not borderless");
    for (const char* invalid : {"2", "true", "false", "off", "exclusive", "Borderless", "00", " 1", "1 ", "-1"}) {
        bool thrown = false;
        try {
            ParseHostFullscreenMode(invalid);
        } catch (const std::runtime_error& error) {
            thrown = std::strstr(error.what(), "ANYPS5_FULLSCREEN") != nullptr;
        }
        Require(thrown, (std::string("invalid value did not throw naming the variable: ") + invalid).c_str());
    }
}

int main() {
    ParseTest();
    return 0;
}
