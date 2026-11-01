#include "prx/libSceFios2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

constexpr int SCE_FIOS_ERROR_INVALID_PATH = static_cast<int>(0x80820002u);
constexpr int SCE_FIOS_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80820006u);

extern "C" {
int APS5_VABI sceFiosStatSync(const char* path, void* buf);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    int retNullPath = sceFiosStatSync(nullptr, nullptr);
    Require(retNullPath == SCE_FIOS_ERROR_INVALID_PATH || retNullPath == SCE_FIOS_ERROR_INVALID_ARGUMENT || retNullPath != 0, "sceFiosStatSync accepted null path");
    return 0;
}
