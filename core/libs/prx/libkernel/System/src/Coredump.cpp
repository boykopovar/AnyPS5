#include <cstdint>
#include <cstddef>
#include <atomic>
#include <cstdio>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// The handler is recorded but never invoked: host crashes are not turned into guest core dumps.
static std::atomic<uint64_t> g_coredumpHandler{0};
static std::atomic<uint64_t> g_coredumpContext{0};

extern "C" {

int APS5_VABI sceCoredumpRegisterCoredumpHandler(uint64_t handler, size_t stack_size, uint64_t context) {
    (void)stack_size;
    g_coredumpHandler.store(handler, std::memory_order_relaxed);
    g_coredumpContext.store(context, std::memory_order_relaxed);
    return 0;
}

int APS5_VABI sceCoredumpUnregisterCoredumpHandler(void) {
    g_coredumpHandler.store(0, std::memory_order_relaxed);
    g_coredumpContext.store(0, std::memory_order_relaxed);
    return 0;
}

int APS5_VABI sceKernelDebugWriteCppExceptionInfo(const void* exception, uint64_t unknown, const char* typeName, const char* what) {
    (void)unknown;
    std::fprintf(stderr, "[coredump] uncaught C++ exception %p of type %s%s%s\n", exception, typeName ? typeName : "(unknown)", what ? ", what(): " : "", what ? what : "");
    return 0;
}


int APS5_VABI sceCoredumpAttachUserFile(void) {
    NotImplemented_nid_no_patch("5nc2gdLNsok");
    return 0;
}

APS5_EXPORT("Jrs7UUkGOFo", sceCoredumpUnknown01);
int APS5_VABI sceCoredumpUnknown01(void) {
    NotImplemented_nid_no_patch("Jrs7UUkGOFo");
    return 0;
}

int APS5_VABI sceCoredumpAttachMemoryRegionAsUserFile(void) {
    NotImplemented_nid_no_patch("MEJ7tc7ThwM");
    return 0;
}

int APS5_VABI sceCoredumpSetUserDataType(void) {
    NotImplemented_nid_no_patch("Uxqkdta7wEg");
    return 0;
}

int APS5_VABI sceCoredumpDebugTextOut(void) {
    NotImplemented_nid_no_patch("dei8oUx6DbU");
    return 0;
}

int APS5_VABI sceCoredumpGetStopInfoCpu(void) {
    NotImplemented_nid_no_patch("kK0DUW1Ukgc");
    return 0;
}
}
