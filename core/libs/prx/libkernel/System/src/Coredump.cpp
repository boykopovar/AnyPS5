#include <cstdint>
#include <cstddef>
#include <atomic>
#include <cstdio>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

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
    return 0;
}

int APS5_VABI sceCoredumpGetStopInfoGpu_Agc(void) {
    return 0;
}

int APS5_VABI sceCoredumpAttachMemoryRegionAsUserFile(void) {
    return 0;
}

int APS5_VABI sceCoredumpSetUserDataType(void) {
    return 0;
}

int APS5_VABI sceCoredumpDebugTextOut(void) {
    return 0;
}

int APS5_VABI sceCoredumpGetStopInfoCpu(void) {
    return 0;
}

int APS5_VABI sceCoredumpWriteUserString() {
    return 0;
}

int APS5_VABI sceCoredumpAttachUserMemoryFile(void) {
    return 0;
}

int APS5_VABI sceCoredumpAttachMemoryRegion(void) {
    return 0;
}
}
