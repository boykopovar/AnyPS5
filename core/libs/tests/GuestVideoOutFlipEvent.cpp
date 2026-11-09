#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "prx/libSceVideoOut/include/Event.hpp"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <stdexcept>

namespace {

bool check(bool condition, const char* reason) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", reason);
        return false;
    }
    return true;
}

} // namespace

int main() {
    bool passed = true;

    KernelEqueue eq = EqueueCreate_nid_postfix();
    passed = check(eq != 0, "EqueueCreate returned valid queue");

    int handle = sceVideoOutOpen(0, 0, 0, nullptr);

    passed = check(sceVideoOutAddFlipEvent(0, handle, nullptr) < 0, "Adding flip event with null queue fails");
    passed = check(sceVideoOutAddFlipEvent(eq, 0, nullptr) < 0, "Adding flip event with invalid port handle fails");

    if (handle >= 0) {
        passed = check(sceVideoOutAddFlipEvent(eq, handle, nullptr) == 0, "Adding flip event with valid handle and queue succeeds");
        passed = check(sceVideoOutAddFlipEvent(eq, handle, nullptr) == 0, "Adding duplicate flip event replaces registration successfully");
        passed = check(sceVideoOutDeleteFlipEvent(eq, handle) == 0, "Deleting registered flip event succeeds");
        sceVideoOutClose(handle);
    }

    EqueueDestroy_nid_postfix(eq);

    if (!passed) {
        return 1;
    }
    std::printf("GuestVideoOutFlipEvent test passed\n");
    return 0;
}
