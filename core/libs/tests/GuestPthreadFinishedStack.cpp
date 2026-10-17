#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <thread>
#include <windows.h>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadDetach(Pthread thread);
int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI scePthreadAttrGet(Pthread thread, PthreadAttr* attr);
int APS5_VABI scePthreadAttrGetstack(const PthreadAttr* attr, void** stackAddress, std::size_t* stackSize);
Pthread APS5_VABI scePthreadSelf();
}

static constexpr int SCE_OK = 0;

static void Require(bool value) { if (!value) std::abort(); }

struct Finished {
    std::atomic<bool> done{false};
    std::uintptr_t stack = 0;
    std::size_t size = 0;
    std::uintptr_t allocation = 0;
};

static std::uintptr_t Allocation(std::uintptr_t address) {
    MEMORY_BASIC_INFORMATION info{};
    Require(VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info)) == sizeof(info));
    return info.State == MEM_COMMIT ? reinterpret_cast<std::uintptr_t>(info.AllocationBase) : 0;
}

static void* APS5_VABI Worker(void* arg) {
    auto& finished = *static_cast<Finished*>(arg);
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    Require(scePthreadAttrGet(scePthreadSelf(), &attr) == SCE_OK);
    void* address = nullptr;
    Require(scePthreadAttrGetstack(&attr, &address, &finished.size) == SCE_OK);
    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    finished.stack = reinterpret_cast<std::uintptr_t>(address);
    const auto local = reinterpret_cast<std::uintptr_t>(&attr);
    Require(local > finished.stack && local < finished.stack + finished.size);
    finished.allocation = Allocation(local);
    Require(finished.allocation != 0);
    finished.done.store(true);
    return arg;
}

static void AwaitFinished(const Finished& finished) {
    while (!finished.done.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

int main() {
    Finished joined;
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Worker, &joined, "finished") == SCE_OK);
    AwaitFinished(joined);
    const auto top = joined.stack + joined.size - 1;
    Require(Allocation(top) == joined.allocation);
    void* result = nullptr;
    Require(scePthreadJoin(thread, &result) == SCE_OK && result == &joined);

    Finished detached;
    Require(scePthreadCreate(&thread, nullptr, Worker, &detached, "detached") == SCE_OK);
    AwaitFinished(detached);
    Require(Allocation(detached.stack + detached.size - 1) == detached.allocation);
    Require(scePthreadDetach(thread) == SCE_OK);
}
