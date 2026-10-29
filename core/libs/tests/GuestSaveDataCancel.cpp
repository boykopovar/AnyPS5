#include "prx/libSceSaveData.native/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

template <typename TCall>
void RequireThrows(TCall call, const std::string& expectedMessage) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()) == expectedMessage, "unexpected error message");
        return;
    }
    Require(false, "call returned instead of reporting that it is not implemented");
}

}

int main() {
    RequireThrows([] { sceSaveDataCancel(0); }, "sceSaveDataCancel not implemented");
    return 0;
}
