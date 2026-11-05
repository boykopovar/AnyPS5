#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <thread>

static constexpr int SigUsr1 = 30;
static void Require(bool value) { if (!value) std::abort(); }
static std::atomic<int> deliveries{0};
static void APS5_VABI Handler(int, void*) { deliveries.fetch_add(1); }

struct Context { PthreadMutex mutex = nullptr; std::atomic<bool> started{false}; std::atomic<bool> acquired{false}; };
static void* APS5_VABI Worker(void* opaque) {
    auto& context = *static_cast<Context*>(opaque);
    context.started.store(true);
    Require(scePthreadMutexLock(&context.mutex) == 0);
    context.acquired.store(true);
    Require(scePthreadMutexUnlock(&context.mutex) == 0);
    return nullptr;
}

int main() {
    Require(sceKernelInstallExceptionHandler(SigUsr1, reinterpret_cast<void*>(&Handler)) == 0);
    Context context;
    Require(scePthreadMutexInit(&context.mutex, nullptr, nullptr) == 0);
    Require(scePthreadMutexLock(&context.mutex) == 0);
    Pthread worker = nullptr;
    Require(scePthreadCreate(&worker, nullptr, Worker, &context, "mutex wait") == 0);
    while (!context.started.load()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    Require(sceKernelRaiseException(worker, SigUsr1) == 0);
    for (int i = 0; i < 5000 && deliveries.load() == 0; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(deliveries.load() == 1);
    Require(scePthreadMutexUnlock(&context.mutex) == 0);
    Require(scePthreadJoin(worker, nullptr) == 0);
    Require(context.acquired.load());
    Require(scePthreadMutexDestroy(&context.mutex) == 0);
    Require(sceKernelRemoveExceptionHandler(SigUsr1) == 0);
}
