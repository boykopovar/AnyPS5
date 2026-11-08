#include "prx/libSceFiber/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool value, const char* what) {
    if (!value) {
        std::fprintf(stderr, "GuestFiberFinalize check failed: %s\n", what);
        std::abort();
    }
}

static void APS5_VABI DummyFiberEntry(uint64_t arg1, uint64_t arg2) {
    (void)arg1;
    (void)arg2;
}

int main() {
    alignas(8) std::array<unsigned char, 0x100> fiberStorage;
    auto* fiber = reinterpret_cast<FiberObject*>(fiberStorage.data());

    alignas(16) std::array<unsigned char, 4096> stackBuffer;

    int32_t ret = sceFiberInitialize_nid_postfix(
        fiber,
        "test_fiber",
        DummyFiberEntry,
        0,
        stackBuffer.data(),
        stackBuffer.size(),
        0
    );
    Require(ret == SCE_OK, "sceFiberInitialize should succeed");

    ret = sceFiberFinalize(fiber);
    Require(ret == SCE_OK, "sceFiberFinalize should succeed first time");

    ret = sceFiberFinalize(fiber);
    Require(ret == static_cast<int32_t>(0x80590004), "sceFiberFinalize should return invalid/null error on finalized/invalid fiber magic");

    ret = sceFiberFinalize(nullptr);
    Require(ret == static_cast<int32_t>(0x80590001), "sceFiberFinalize should return null error for nullptr");

    return 0;
}
