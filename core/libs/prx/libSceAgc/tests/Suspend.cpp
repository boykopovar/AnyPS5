#include "prx/libSceAgc/Misc/include/Suspend.hpp"

#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename TAction>
void expectRejection(TAction action, std::uint32_t mode) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        check(message.find("sceAgcSetSubmitMode") != std::string::npos, "rejection does not name the function");
        check(message.find("invalid submit mode") != std::string::npos, "rejection does not name the invalid mode");
        return;
    }
    throw std::runtime_error("submit mode " + std::to_string(mode) + " was accepted");
}

void testModeZero() {
    check(sceAgcSetSubmitMode(0) == 0, "mode 0 did not return 0");
    check(sceAgcSetSubmitMode(0) == 0, "repeating mode 0 did not return 0");
}

void testRejectedModes() {
    for (const std::uint32_t mode : {1u, 2u, 0x20000u, 0xffffffffu}) {
        expectRejection([mode] { sceAgcSetSubmitMode(mode); }, mode);
    }
}

}

int main() {
    try {
        testModeZero();
        testRejectedModes();
        std::puts("AGC submit mode tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
