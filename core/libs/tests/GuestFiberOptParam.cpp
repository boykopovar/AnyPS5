#include "prx/libSceFiber/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool value, const char* what) {
    if (!value) {
        std::fprintf(stderr, "GuestFiberOptParam check failed: %s\n", what);
        std::abort();
    }
}

int main() {
    FiberOptParam optParam;
    std::memset(&optParam, 0xff, sizeof(optParam));

    int32_t ret = sceFiberOptParamInitialize(&optParam);
    Require(ret == SCE_OK, "sceFiberOptParamInitialize should succeed with valid pointer");

    std::uint8_t raw[sizeof(FiberOptParam)];
    std::memcpy(raw, &optParam, sizeof(raw));
    for (std::size_t i = 0; i < sizeof(FiberOptParam); ++i) {
        Require(raw[i] == 0, "sceFiberOptParamInitialize must zero-initialize the structure bytes");
    }

    ret = sceFiberOptParamInitialize(nullptr);
    Require(ret == SCE_FIBER_ERROR_NULL, "sceFiberOptParamInitialize should return null error for nullptr");

    return 0;
}
