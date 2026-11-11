#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition) {
    if (condition) return;
    std::abort();
}

void TestZeroArguments() {
    volatile int step = 0;
    sceKernelDebugRaiseException(0, 0);
    step = 1;
    Require(step == 1);

    sceKernelDebugRaiseExceptionOnReleaseMode(0, 0);
    step = 2;
    Require(step == 2);
}

void TestLibcExceptionCodes() {
    volatile int step = 0;
    sceKernelDebugRaiseException(static_cast<int>(0xa0020001u), 0);
    step = 1;
    Require(step == 1);

    sceKernelDebugRaiseException(static_cast<int>(0xa002ffffu), 0);
    step = 2;
    Require(step == 2);

    sceKernelDebugRaiseExceptionOnReleaseMode(static_cast<int>(0xa0020001u), 0);
    step = 3;
    Require(step == 3);

    sceKernelDebugRaiseExceptionOnReleaseMode(static_cast<int>(0xa002ffffu), 0);
    step = 4;
    Require(step == 4);
}

void TestBoundaryArguments() {
    volatile int step = 0;
    sceKernelDebugRaiseException(-1, -1);
    step = 1;
    Require(step == 1);

    sceKernelDebugRaiseException(INT32_MIN, INT32_MAX);
    step = 2;
    Require(step == 2);

    sceKernelDebugRaiseExceptionOnReleaseMode(-1, -1);
    step = 3;
    Require(step == 3);

    sceKernelDebugRaiseExceptionOnReleaseMode(INT32_MIN, INT32_MAX);
    step = 4;
    Require(step == 4);
}

void TestArbitraryCodes() {
    volatile int step = 0;
    sceKernelDebugRaiseException(static_cast<int>(0x12345678u), static_cast<int>(0x9abcdef0u));
    step = 1;
    Require(step == 1);

    sceKernelDebugRaiseExceptionOnReleaseMode(static_cast<int>(0x12345678u), static_cast<int>(0x9abcdef0u));
    step = 2;
    Require(step == 2);
}

void TestSequentialPreservation() {
    volatile int counter = 0;
    for (int i = 0; i < 10; ++i) {
        sceKernelDebugRaiseException(i, i * 2);
        sceKernelDebugRaiseExceptionOnReleaseMode(i * 3, i * 4);
        counter += 1;
    }
    Require(counter == 10);
}

} // namespace

int main() {
    TestZeroArguments();
    TestLibcExceptionCodes();
    TestBoundaryArguments();
    TestArbitraryCodes();
    TestSequentialPreservation();
    std::puts("GuestKernelDebugRaiseException passed");
    return 0;
}
