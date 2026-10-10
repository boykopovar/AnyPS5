#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <stdexcept>
#include <vector>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
Pthread APS5_VABI scePthreadSelf();
int APS5_VABI _sceLibcInternalThreadAtexit_nid_postfix(void (APS5_VABI* destructor)(void*), void* object, void* dsoSymbol);
void APS5_VABI _sceLibcInternalThreadDtors_nid_postfix();
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

using Destructor = void (APS5_VABI*)(void*);

int dsoHandle = 0;
std::vector<std::intptr_t> calls;
std::vector<Pthread> callers;
int failedCallbackRegistrations = 0;

void* Object(std::intptr_t value) { return reinterpret_cast<void*>(value); }

int Register(Destructor destructor, std::intptr_t object) {
    return _sceLibcInternalThreadAtexit_nid_postfix(destructor, Object(object), &dsoHandle);
}

void APS5_VABI Record(void* object) {
    calls.push_back(reinterpret_cast<std::intptr_t>(object));
    callers.push_back(scePthreadSelf());
}

void APS5_VABI RecordAndRegister(void* object) {
    Record(object);
    if (Register(Record, 9) != 0) ++failedCallbackRegistrations;
}

void* APS5_VABI Worker(void*) {
    if (Register(Record, 4) != 0) ++failedCallbackRegistrations;
    if (Register(Record, 5) != 0) ++failedCallbackRegistrations;
    return nullptr;
}

bool DestructorsThrow() {
    try {
        _sceLibcInternalThreadDtors_nid_postfix();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

class ThreadDestructorFixture {
public:
    ThreadDestructorFixture() { Reset(); }

    ~ThreadDestructorFixture() {
        for (int attempt = 0; attempt < 64; ++attempt) {
            if (!DrainThrows()) break;
        }
        Reset();
    }

    ThreadDestructorFixture(const ThreadDestructorFixture&) = delete;
    ThreadDestructorFixture& operator=(const ThreadDestructorFixture&) = delete;

private:
    static bool DrainThrows() {
        try {
            _sceLibcInternalThreadDtors_nid_postfix();
        } catch (...) {
            return true;
        }
        return false;
    }

    static void Reset() {
        calls.clear();
        callers.clear();
        failedCallbackRegistrations = 0;
    }
};

void RegisterMainThreadSequence() {
    RequireEqual(Register(Record, 1), 0, "register 1");
    RequireEqual(Register(RecordAndRegister, 2), 0, "register 2");
    RequireEqual(Register(Record, 3), 0, "register 3");
}

const Case mainThread{"ThreadDtors_MainThread_RunsInReverseOrderIncludingNestedRegistration", [] {
    const ThreadDestructorFixture fixture;
    const Pthread mainThread = scePthreadSelf();
    RegisterMainThreadSequence();
    _sceLibcInternalThreadDtors_nid_postfix();
    Require(calls == std::vector<std::intptr_t>{3, 2, 9, 1}, "destructor order 3, 2, 9, 1");
    Require(callers == std::vector<Pthread>(4, mainThread), "every destructor ran on the main thread");
    RequireEqual(failedCallbackRegistrations, 0, "failed registrations inside destructors");
}};

const Case calledAgain{"ThreadDtors_CalledAgain_RunsNothing", [] {
    const ThreadDestructorFixture fixture;
    RegisterMainThreadSequence();
    _sceLibcInternalThreadDtors_nid_postfix();
    _sceLibcInternalThreadDtors_nid_postfix();
    RequireEqual(calls.size(), std::size_t {4}, "destructor calls after the second pass");
}};

const Case workerThread{"ThreadDtors_WorkerThreadExit_RunsWorkerDestructorsOnWorker", [] {
    const ThreadDestructorFixture fixture;
    Pthread worker = nullptr;
    RequireEqual(scePthreadCreate(&worker, nullptr, Worker, nullptr, "ThreadAtexit"), 0, "create the worker");
    RequireEqual(scePthreadJoin(worker, nullptr), 0, "join the worker");
    RequireEqual(failedCallbackRegistrations, 0, "failed registrations on the worker");
    Require(calls == std::vector<std::intptr_t>{5, 4}, "destructor order 5, 4");
    Require(callers == std::vector<Pthread>(2, worker), "every destructor ran on the worker");
}};

const Case invalidDestructors{"ThreadDtors_DestructorsOutsideLoadedImage_ThrowUntilDroppedThenValidOneRuns", [] {
    std::vector<unsigned char> heap(64);
    const ThreadDestructorFixture fixture;
    RequireEqual(Register(Record, 6), 0, "register 6");
    RequireEqual(Register(nullptr, 7), 0, "register a null destructor");
    RequireEqual(Register(reinterpret_cast<Destructor>(heap.data()), 8), 0, "register a heap destructor");
    Require(DestructorsThrow(), "first pass throws");
    Require(DestructorsThrow(), "second pass throws");
    Require(calls.empty(), "no destructor ran while invalid entries remained");
    _sceLibcInternalThreadDtors_nid_postfix();
    Require(calls == std::vector<std::intptr_t>{6}, "the valid destructor runs afterwards");
}};

} // namespace
