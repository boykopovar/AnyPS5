#include "SceTypes.hpp"
#include <atomic>
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI sceKernelIsStack(void* addr, void** start, void** end);
int APS5_VABI sceKernelMapFlexibleMemory(void** addr, std::size_t len, int prot, int flags);
int APS5_VABI sceKernelMunmap(std::uint64_t addr, std::size_t len);
}

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EACCES = static_cast<int>(0x8002000d);
static int global = 0;

static void Require(bool value) { if (!value) std::abort(); }

struct WorkerContext {
    std::atomic<bool> queried{false};
    std::atomic<bool> release{false};
    void* local = nullptr;
    void* start = nullptr;
    void* end = nullptr;
    int result = -1;
};

static void* APS5_VABI Worker(void* arg) {
    auto& context = *static_cast<WorkerContext*>(arg);
    int local = 0;
    context.local = &local;
    context.result = sceKernelIsStack(&local, &context.start, &context.end);
    context.queried.store(true);
    while (!context.release.load()) {}
    return nullptr;
}

int main() {
    int local = 0;
    void* start = nullptr;
    void* end = nullptr;
    Require(sceKernelIsStack(&local, &start, &end) == SCE_OK);
    Require(start <= static_cast<void*>(&local) && static_cast<void*>(&local) < end);
    Require(sceKernelIsStack(&local, nullptr, nullptr) == SCE_OK);

    start = &local;
    end = &local;
    Require(sceKernelIsStack(&global, &start, &end) == SCE_OK);
    Require(start == nullptr && end == nullptr);

    WorkerContext context;
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Worker, &context, nullptr) == SCE_OK);
    while (!context.queried.load()) {}
    Require(context.result == SCE_OK);
    Require(context.start <= context.local && context.local < context.end);
    Require(sceKernelIsStack(&local, &start, &end) == SCE_OK);
    Require(context.end <= start || context.start >= end);
    void* workerStart = nullptr;
    void* workerEnd = nullptr;
    Require(sceKernelIsStack(context.local, &workerStart, &workerEnd) == SCE_OK);
    Require(workerStart == context.start && workerEnd == context.end);
    context.release.store(true);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);

    constexpr std::size_t page = 0x4000;
    void* mapped = nullptr;
    Require(sceKernelMapFlexibleMemory(&mapped, page, 3, 0) == SCE_OK);
    Require(sceKernelIsStack(mapped, &start, &end) == SCE_OK);
    Require(start == nullptr && end == nullptr);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(mapped), page) == SCE_OK);
    Require(sceKernelIsStack(mapped, &start, &end) == SCE_KERNEL_ERROR_EACCES);
}
