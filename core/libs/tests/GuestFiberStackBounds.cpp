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
        std::fprintf(stderr, "GuestFiberStackBounds check failed: %s\n", what);
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
    Require(ret == SCE_OK, "sceFiberInitialize should succeed with valid stack and size");

    ret = sceFiberFinalize(fiber);
    Require(ret == SCE_OK, "sceFiberFinalize should succeed");

    ret = sceFiberInitialize_nid_postfix(
        fiber,
        "test_fiber_small",
        DummyFiberEntry,
        0,
        stackBuffer.data(),
        256,
        0
    );
    Require(ret == static_cast<int32_t>(0x80590003), "sceFiberInitialize should return range error for context size below 512");

    ret = sceFiberInitialize_nid_postfix(
        fiber,
        "test_fiber_unaligned_addr",
        DummyFiberEntry,
        0,
        stackBuffer.data() + 1,
        4096,
        0
    );
    Require(ret == static_cast<int32_t>(0x80590002), "sceFiberInitialize should return alignment error for unaligned stack address");

    ret = sceFiberInitialize_nid_postfix(
        fiber,
        "test_fiber_unaligned_size",
        DummyFiberEntry,
        0,
        stackBuffer.data(),
        4095,
        0
    );
    Require(ret == static_cast<int32_t>(0x80590002), "sceFiberInitialize should return alignment error for unaligned stack size");

    ret = sceFiberInitialize_nid_postfix(
        fiber,
        "test_fiber_null_addr",
        DummyFiberEntry,
        0,
        nullptr,
        4096,
        0
    );
    Require(ret == static_cast<int32_t>(0x80590004), "sceFiberInitialize should return invalid error for null stack address with non-zero size");

    return 0;
}
