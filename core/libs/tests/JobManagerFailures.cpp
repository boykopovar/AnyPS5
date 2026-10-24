#include "SceTypes.hpp"
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <new>
#include <stdexcept>
#include <thread>

namespace {

int failure = 0;
int calls = 0;
int attributes = 0;
int threads = 0;
int queues = 0;
int registrations = 0;

bool Fail() { return ++calls == failure; }

void Check(bool condition) {
    if (!condition) std::abort();
}

struct EventQueue {
    std::mutex lock;
    std::condition_variable ready;
    bool triggered = false;
};

}

extern "C" {

int APS5_VABI scePthreadAttrInit(PthreadAttr* value) {
    if (Fail()) return 12;
    *value = reinterpret_cast<PthreadAttr>(new int);
    ++attributes;
    return 0;
}

int APS5_VABI scePthreadAttrDestroy(PthreadAttr* value) {
    delete reinterpret_cast<int*>(*value);
    *value = nullptr;
    --attributes;
    return 0;
}

int APS5_VABI scePthreadAttrSetinheritsched(PthreadAttr*, int) { return Fail() ? 22 : 0; }
int APS5_VABI scePthreadAttrSetschedparam(PthreadAttr*, const KernelSchedParam*) { return Fail() ? 22 : 0; }
int APS5_VABI scePthreadAttrSetstacksize(PthreadAttr*, std::size_t) { return Fail() ? 22 : 0; }
int APS5_VABI scePthreadAttrSetaffinity(PthreadAttr*, KernelCpumask) { return Fail() ? 22 : 0; }

int APS5_VABI scePthreadCreate(Pthread* value, const PthreadAttr*, PthreadEntry entry, void* argument, const char*) {
    if (Fail()) return 11;
    *value = reinterpret_cast<Pthread>(new std::thread([=] { entry(argument); }));
    ++threads;
    return 0;
}

int APS5_VABI scePthreadJoin(Pthread value, void**) {
    auto* thread = reinterpret_cast<std::thread*>(value);
    thread->join();
    delete thread;
    --threads;
    return 0;
}

int APS5_VABI sceKernelCreateEqueue(KernelEqueue* value, const char*) {
    if (Fail()) return 12;
    *value = reinterpret_cast<KernelEqueue>(new EventQueue);
    ++queues;
    return 0;
}

int APS5_VABI sceKernelDeleteEqueue(KernelEqueue value) {
    delete reinterpret_cast<EventQueue*>(value);
    --queues;
    return 0;
}

int APS5_VABI sceKernelWaitEqueue(KernelEqueue value, KernelEvent* event, int, int* count, const KernelUseconds*) {
    auto& queue = *reinterpret_cast<EventQueue*>(value);
    std::unique_lock guard(queue.lock);
    queue.ready.wait(guard, [&queue] { return queue.triggered; });
    event->filter = -11;
    event->ident = 0;
    *count = 1;
    return 0;
}

int APS5_VABI sceKernelAddUserEventEdge(KernelEqueue, int) { return 0; }

int APS5_VABI sceKernelTriggerUserEvent(KernelEqueue value, int, void*) {
    auto& queue = *reinterpret_cast<EventQueue*>(value);
    const std::lock_guard guard(queue.lock);
    queue.triggered = true;
    queue.ready.notify_all();
    return 0;
}

int APS5_VABI sceKernelGetEventFilter(const KernelEvent* event) { return event->filter; }
std::uintptr_t APS5_VABI sceKernelGetEventId(const KernelEvent* event) { return event->ident; }

int APS5_VABI sceAgcDriverAddEqEvent(KernelEqueue, int, void*) {
    if (Fail()) return 22;
    ++registrations;
    return 0;
}

int APS5_VABI sceAgcDriverDeleteEqEvent(KernelEqueue, int) {
    --registrations;
    return 0;
}

void APS5_VABI _ZN3sce3Job10JobManagerC1Ev(void*);
int APS5_VABI _ZN3sce3Job10JobManager10initializeEPvmjjjmPKNS1_12ConfigParamsE(void*, void*, std::uint64_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint64_t, const void*);
int APS5_VABI _ZN3sce3Job10JobManager8shutdownEv(void*);
void* APS5_VABI _ZN3sce3Job10JobManager27getSequenceFactoryInterfaceENS1_8PriorityE(void*, std::uint32_t);

}

int main() {
    constexpr std::size_t size = 1024 * 1024;
    void* memory = ::operator new(size, std::align_val_t{256});
    for (const std::uint32_t computeSequences : {0u, 2u}) {
        int failures = 0;
        for (failure = 1; failure < 40; ++failure) {
            alignas(64) unsigned char manager[0x100]{};
            _ZN3sce3Job10JobManagerC1Ev(manager);
            calls = 0;
            bool threw = false;
            try {
                Check(_ZN3sce3Job10JobManager10initializeEPvmjjjmPKNS1_12ConfigParamsE(manager, memory, size, 3, 2, computeSequences, computeSequences ? 5 : 0, nullptr) == 0);
            } catch (const std::runtime_error&) {
                threw = true;
                ++failures;
            }
            if (threw) {
                Check(_ZN3sce3Job10JobManager27getSequenceFactoryInterfaceENS1_8PriorityE(manager, 0) == nullptr);
                Check(_ZN3sce3Job10JobManager8shutdownEv(manager) == static_cast<int>(0x81581010u));
            } else {
                Check(_ZN3sce3Job10JobManager8shutdownEv(manager) == 0);
            }
            Check(attributes == 0 && threads == 0 && queues == 0 && registrations == 0);
            if (!threw) break;
            const int injected = failure;
            failure = 0;
            Check(_ZN3sce3Job10JobManager10initializeEPvmjjjmPKNS1_12ConfigParamsE(manager, memory, size, 3, 2, computeSequences, computeSequences ? 5 : 0, nullptr) == 0);
            Check(_ZN3sce3Job10JobManager8shutdownEv(manager) == 0);
            Check(attributes == 0 && threads == 0 && queues == 0 && registrations == 0);
            failure = injected;
        }
        Check(failures == (computeSequences ? 27 : 18));
        std::printf("initialization failures recovered: %d\n", failures);
    }
    ::operator delete(memory, std::align_val_t{256});
}
