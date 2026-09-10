#include "prx/libkernel/Pthread/Pthread.hpp"
#include <future>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

extern "C" int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
extern "C" int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
extern "C" int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
extern "C" int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
extern "C" int APS5_VABI scePthreadAttrGet(Pthread thread, PthreadAttr* attr);
extern "C" int APS5_VABI scePthreadAttrSetstacksize(PthreadAttr* attr, std::size_t size);
extern "C" int APS5_VABI scePthreadAttrGetstack(const PthreadAttr* attr, void** address, std::size_t* size);
extern "C" Pthread APS5_VABI scePthreadSelf();
extern "C" void APS5_VABI scePthreadExit(void* retval);
extern "C" int APS5_VABI scePthreadDetach(Pthread thread);

struct ThreadContext {
    Pthread thread = nullptr;
    std::promise<void> checked;
    std::size_t stackSize = 0;
    bool exitExplicitly = false;
};

static void CheckLargeFrame() {
    volatile unsigned char bytes[0x9000];
    bytes[0] = 19;
    bytes[sizeof(bytes) - 1] = 37;
    if (bytes[0] != 19 || bytes[sizeof(bytes) - 1] != 37)
        throw std::runtime_error("Large guest stack frame was corrupted");
}

static void* APS5_VABI CheckThread(void* arg) {
    auto& context = *static_cast<ThreadContext*>(arg);
    if (!context.thread) throw std::runtime_error("Thread handle was not published");
#ifdef _WIN32
    if (scePthreadSelf() != context.thread)
        throw std::runtime_error("Current guest thread handle is incorrect");
    if (context.thread->threadId != std::this_thread::get_id() || !context.thread->nativeHandle)
        throw std::runtime_error("Native thread was not initialized");
#else
    if (context.thread->_thr.get_id() != std::this_thread::get_id()) throw std::runtime_error("Thread object was not initialized");
    if (!context.thread->_thr.joinable()) throw std::runtime_error("Thread object is not joinable");
#endif
    PthreadAttr attr = nullptr;
    if (scePthreadAttrInit(&attr) != 0) throw std::runtime_error("Attribute initialization failed");
    if (scePthreadAttrGet(context.thread, &attr) != 0) throw std::runtime_error("Attribute query failed");
#ifdef _WIN32
    void* address = nullptr;
    std::size_t size = 0;
    if (scePthreadAttrGetstack(&attr, &address, &size) != 0 || !address || size != context.stackSize)
        throw std::runtime_error("Guest stack attributes do not match the request");
    const auto begin = reinterpret_cast<std::uintptr_t>(address);
    const auto local = reinterpret_cast<std::uintptr_t>(&size);
    if (local < begin || local >= begin + size)
        throw std::runtime_error("Callback is outside the reported stack");
    for (auto cursor = begin; cursor < begin + size;) {
        MEMORY_BASIC_INFORMATION memory{};
        if (VirtualQuery(reinterpret_cast<void*>(cursor), &memory, sizeof(memory)) != sizeof(memory) || memory.State != MEM_COMMIT || memory.Protect != PAGE_READWRITE)
            throw std::runtime_error("Guest stack contains an inaccessible page");
        cursor = reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize;
    }
#endif
    CheckLargeFrame();
    if (scePthreadAttrDestroy(&attr) != 0) throw std::runtime_error("Attribute destruction failed");
    context.checked.set_value();
    if (context.exitExplicitly)
        scePthreadExit(arg);
    return arg;
}

int main() {
    for (int iteration = 0; iteration < 10000; ++iteration) {
        ThreadContext context;
        context.stackSize = (iteration % 2 + 1) * (1u << 20);
        context.exitExplicitly = iteration % 3 == 0;
        PthreadAttr attr = nullptr;
        if (scePthreadAttrInit(&attr) != 0 || scePthreadAttrSetstacksize(&attr, context.stackSize) != 0)
            throw std::runtime_error("Requested stack setup failed");
        if (scePthreadCreate(&context.thread, &attr, CheckThread, &context, nullptr) != 0) throw std::runtime_error("Thread creation failed");
        if (scePthreadAttrDestroy(&attr) != 0)
            throw std::runtime_error("Requested stack cleanup failed");
        context.checked.get_future().get();
        void* result = nullptr;
        if (scePthreadJoin(context.thread, &result) != 0) throw std::runtime_error("Thread join failed");
        if (result != &context) throw std::runtime_error("Thread return value was lost");
    }
#ifdef _WIN32
    for (int iteration = 0; iteration < 100; ++iteration) {
        ThreadContext context;
        context.stackSize = 1u << 20;
        if (scePthreadCreate(&context.thread, nullptr, CheckThread, &context, nullptr) != 0)
            throw std::runtime_error("Detach test creation failed");
        HANDLE handle = nullptr;
        if (!DuplicateHandle(GetCurrentProcess(), context.thread->nativeHandle, GetCurrentProcess(), &handle, SYNCHRONIZE, FALSE, 0))
            throw std::runtime_error("Detach test handle duplication failed");
        if (scePthreadDetach(context.thread) != 0 || WaitForSingleObject(handle, 30000) != WAIT_OBJECT_0 || !CloseHandle(handle))
            throw std::runtime_error("Detached thread did not finish correctly");
        context.checked.get_future().get();
    }
#endif
    std::cout << "PASS: 10000 thread starts, committed stacks, large frames, explicit exits and joins; 100 detach checks\n";
}
