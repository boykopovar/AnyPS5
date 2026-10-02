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

APS5_EXPORT("DoKHmUw1yiQ", sceCoredumpUnknown00);
int APS5_VABI sceCoredumpUnknown00() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("MEJ7tc7ThwM", sceCoredumpUnknown01);
int APS5_VABI sceCoredumpUnknown01() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("Uxqkdta7wEg", sceCoredumpUnknown02);
int APS5_VABI sceCoredumpUnknown02() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("+YX0z-GUSNw", sceCoredumpUnknown03);
int APS5_VABI sceCoredumpUnknown03() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
