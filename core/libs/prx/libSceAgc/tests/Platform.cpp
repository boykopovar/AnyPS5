#include "prx/libSceAgc/Misc/include/Platform.hpp"
#include "prx/libSceAgc/include/Agc.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "Testing.hpp"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace {

template <typename TAction>
void expectFailure(TAction action) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        Require(error.what()[0] != '\0', "empty exception message");
        return;
    }
    throw std::runtime_error("expected an exception");
}

void testTrinityMode() {
    std::array<bool, 3> flags{true, true, true};
    Require(sceAgcGetIsTrinityMode(&flags[1]) == 0, "Trinity mode query did not return 0");
    Require(!flags[1], "base PS5 GPU reported as Trinity");
    Require(flags[0] && flags[2], "Trinity mode query wrote past its one-byte flag");
}

void testRejections() {
    expectFailure([] { sceAgcGetIsTrinityMode(nullptr); });
    expectFailure([] { sceAgcGetIsTrinityMode(reinterpret_cast<bool*>(std::uintptr_t{0x10})); });
}

void testExportLinked() {
    auto* volatile linked = &sceAgcSuspendPointAndCheckStatus;
    Require(linked != nullptr, "sceAgcSuspendPointAndCheckStatus is not linked");
}

}

int main() {
    try {
        testTrinityMode();
        testRejections();
        testExportLinked();
        LibcRunShutdown_nid_postfix();
        std::puts("AGC platform tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        return 1;
    }
}
