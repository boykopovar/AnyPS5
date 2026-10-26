#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <cstdint>
#include <cstdlib>

static constexpr int SCE_OK = 0;
#ifdef ANYPS5_ENABLE_ASAN
static constexpr std::uintptr_t APPLICATION_AREA_END = 0x000010FC00000000ull;
#else
static constexpr std::uintptr_t APPLICATION_AREA_END = 0x000000FC00000000ull;
#endif
static int global = 0;

static void Require(bool value) { if (!value) std::abort(); }

struct WorkerContext {
    std::atomic<bool> queried{false};
    std::atomic<bool> release{false};
    void* local = nullptr;
    void* start = nullptr;
    void* end = nullptr;
    int result = -1;
    VirtualQueryInfo region{};
    int regionResult = -1;
};

static void* APS5_VABI Worker(void* arg) {
    auto& context = *static_cast<WorkerContext*>(arg);
    int local = 0;
    context.local = &local;
    context.result = sceKernelIsStack(&local, &context.start, &context.end);
    context.regionResult = sceKernelVirtualQuery(&local, 0, &context.region, sizeof(context.region));
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
    Require(context.regionResult == SCE_OK);
    Require(context.region.start <= reinterpret_cast<std::uintptr_t>(context.local) && reinterpret_cast<std::uintptr_t>(context.local) < context.region.end);
#if defined(__linux__)
    Require(reinterpret_cast<std::uintptr_t>(context.local) < APPLICATION_AREA_END);
#endif
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
