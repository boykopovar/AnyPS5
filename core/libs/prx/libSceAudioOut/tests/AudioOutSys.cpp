#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdio>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceAudioOutSysClose();
int APS5_VABI sceAudioOutSysConfigureOutput();
int APS5_VABI sceAudioOutSysGetHdmiMonitorInfo();
int APS5_VABI sceAudioOutSysOpen();
}

using SysFunction = int (APS5_VABI *)();

static bool ReportsNotImplemented(SysFunction function, const char* name) {
    try {
        function();
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()) == std::string(name) + " not implemented") return true;
        std::fprintf(stderr, "%s threw: %s\n", name, error.what());
        return false;
    }
    std::fprintf(stderr, "%s returned instead of reporting that it is not implemented\n", name);
    return false;
}

int main() {
    bool passed = ReportsNotImplemented(sceAudioOutSysClose, "sceAudioOutSysClose");
    passed = ReportsNotImplemented(sceAudioOutSysConfigureOutput, "sceAudioOutSysConfigureOutput") && passed;
    passed = ReportsNotImplemented(sceAudioOutSysGetHdmiMonitorInfo, "sceAudioOutSysGetHdmiMonitorInfo") && passed;
    passed = ReportsNotImplemented(sceAudioOutSysOpen, "sceAudioOutSysOpen") && passed;
    return passed ? 0 : 1;
}
