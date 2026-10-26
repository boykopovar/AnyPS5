#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>

static bool ReportsNotImplemented(const std::string& name, const std::function<int()>& call) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        if (error.what() == name + " not implemented") return true;
        std::fprintf(stderr, "%s threw: %s\n", name.c_str(), error.what());
        return false;
    }
    std::fprintf(stderr, "%s returned instead of reporting that it is not implemented\n", name.c_str());
    return false;
}

int main() {
    static const std::uint32_t image[64 * 64] = {};
    bool passed = ReportsNotImplemented("sceVideoOutCursorEnable", [] { return sceVideoOutCursorEnable(1, 0, image); });
    passed = ReportsNotImplemented("sceVideoOutCursorSetImageAddress", [] { return sceVideoOutCursorSetImageAddress(1, 0, image); }) && passed;
    passed = ReportsNotImplemented("sceVideoOutCursorSetPosition", [] { return sceVideoOutCursorSetPosition(1, 0, 960, 540); }) && passed;
    passed = ReportsNotImplemented("sceVideoOutCursorDisable", [] { return sceVideoOutCursorDisable(1, 0); }) && passed;
    return passed ? 0 : 1;
}
