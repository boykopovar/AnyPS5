#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceVoiceSetMuteFlag();
int APS5_VABI sceVoiceGetMuteFlag();
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    bool setNotImplemented = false;
    try {
        sceVoiceSetMuteFlag();
    } catch (const std::runtime_error& error) {
        setNotImplemented = std::string(error.what()) == "sceVoiceSetMuteFlag not implemented";
    }
    Require(setNotImplemented);

    bool getNotImplemented = false;
    try {
        sceVoiceGetMuteFlag();
    } catch (const std::runtime_error& error) {
        getNotImplemented = std::string(error.what()) == "sceVoiceGetMuteFlag not implemented";
    }
    Require(getNotImplemented);
}
