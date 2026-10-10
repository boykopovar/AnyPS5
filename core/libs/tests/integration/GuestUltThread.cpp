#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <thread>

extern "C" {
int APS5_VABI sceUltInitialize();
int APS5_VABI sceUltFinalize();
std::uint64_t APS5_VABI sceUltUlthreadRuntimeGetWorkAreaSize(std::uint32_t, std::uint32_t);
int APS5_VABI sceUltUlthreadRuntimeCreate(void*, const char*, std::uint32_t, std::uint32_t, void*, const void*, std::uint32_t);
int APS5_VABI sceUltUlthreadCreate(void*, const char*, UltUlthreadEntry, std::uint64_t, void*, std::uint64_t, void*, const void*, std::uint32_t);
int APS5_VABI sceUltUlthreadJoin(void*, std::int32_t*);
int APS5_VABI sceUltUlthreadTryJoin(void*, std::int32_t*);
int APS5_VABI sceUltUlthreadRuntimeDestroy(void*);
int APS5_VABI _sceUltUlthreadRuntimeOptParamInitialize(void*, std::uint32_t);
int APS5_VABI _sceUltUlthreadRuntimeCreate(void*, const char*, std::uint32_t, std::uint32_t, void*, const void*, std::uint32_t);
int APS5_VABI _sceUltUlthreadCreate(void*, const char*, UltUlthreadEntry, std::uint64_t, void*, std::uint64_t, void*, const void*, std::uint32_t);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

constexpr int Ok = 0;
constexpr int Null = -2139029503;
constexpr int State = -2139029498;
constexpr int Busy = -2139029497;

std::atomic<bool> started{false};
std::atomic<bool> released{false};

std::int32_t APS5_VABI Parked(std::uint64_t arg) {
    started.store(true, std::memory_order_release);
    while (!released.load(std::memory_order_acquire)) std::this_thread::yield();
    return static_cast<std::int32_t>(arg);
}

std::int32_t APS5_VABI Immediate(std::uint64_t arg) {
    return static_cast<std::int32_t>(arg);
}

void WaitUntilStarted() {
    while (!started.load(std::memory_order_acquire)) std::this_thread::yield();
}

struct alignas(8) Runtime {
    std::uint8_t bytes[4096]{};
};

struct alignas(8) Thread {
    std::uint8_t bytes[512]{};
};

class UltFixture {
public:
    UltFixture() {
        started.store(false, std::memory_order_release);
        released.store(false, std::memory_order_release);
        RequireEqual(sceUltInitialize(), Ok, "initialize the ULT library");
    }

    ~UltFixture() {
        released.store(true, std::memory_order_release);
        sceUltUlthreadJoin(&thread, nullptr);
        sceUltFinalize();
    }

    UltFixture(const UltFixture&) = delete;
    UltFixture& operator=(const UltFixture&) = delete;

    Runtime runtime;
    Runtime other;
    Thread thread;
};

const Case outstanding{"Finalize_OutstandingUlthread_ReturnsBusyAndThreadStaysJoinable", [] {
    UltFixture fixture;
    RequireEqual(sceUltUlthreadRuntimeCreate(&fixture.runtime, "runtime", 1, 1, nullptr, nullptr, 0), Ok, "create the runtime");
    RequireEqual(sceUltUlthreadCreate(&fixture.thread, "parked", Parked, 0x5A, nullptr, 0, &fixture.runtime, nullptr, 0), Ok, "create the parked ulthread");
    WaitUntilStarted();

    const int busy = sceUltFinalize();
    released.store(true, std::memory_order_release);
    std::int32_t status = 0;
    const int joined = sceUltUlthreadJoin(&fixture.thread, &status);

    RequireEqual(busy, Busy, "finalize with a running ulthread");
    RequireEqual(joined, Ok, "join after release");
    RequireEqual(status, std::int32_t{0x5A}, "exit status");
    RequireEqual(sceUltUlthreadJoin(&fixture.thread, &status), State, "join an already joined ulthread");
    RequireEqual(sceUltFinalize(), Ok, "finalize after join");
}};

const Case runtimeCreateNull{"UlthreadRuntimeCreate_NullRuntime_ReturnsNull", [] {
    const UltFixture fixture;
    RequireEqual(sceUltUlthreadRuntimeCreate(nullptr, "runtime", 1, 1, nullptr, nullptr, 0), Null, "null runtime");
}};

const Case createNullArguments{"UlthreadCreate_NullArguments_ReturnNull", [] {
    UltFixture fixture;
    RequireEqual(sceUltUlthreadRuntimeCreate(&fixture.runtime, "runtime", 1, 1, nullptr, nullptr, 0), Ok, "create the runtime");
    RequireEqual(sceUltUlthreadCreate(nullptr, "thread", Immediate, 0, nullptr, 0, &fixture.runtime, nullptr, 0), Null, "null ulthread");
    RequireEqual(sceUltUlthreadCreate(&fixture.thread, "thread", nullptr, 0, nullptr, 0, &fixture.runtime, nullptr, 0), Null, "null entry");
    RequireEqual(sceUltUlthreadCreate(&fixture.thread, "thread", Immediate, 0, nullptr, 0, nullptr, nullptr, 0), Null, "null runtime");
}};

const Case joinInvalid{"UlthreadJoin_NullOrUnregisteredThread_ReturnsNullOrState", [] {
    UltFixture fixture;
    RequireEqual(sceUltUlthreadJoin(nullptr, nullptr), Null, "join a null ulthread");
    RequireEqual(sceUltUlthreadJoin(&fixture.thread, nullptr), State, "join an unregistered ulthread");
}};

const Case createDuplicate{"UlthreadCreate_AlreadyRegisteredThread_ReturnsState", [] {
    UltFixture fixture;
    RequireEqual(sceUltUlthreadRuntimeCreate(&fixture.runtime, "runtime", 1, 1, nullptr, nullptr, 0), Ok, "create the runtime");
    RequireEqual(sceUltUlthreadCreate(&fixture.thread, "thread", Immediate, 0x27, nullptr, 0, &fixture.runtime, nullptr, 0), Ok, "create the ulthread");
    RequireEqual(sceUltUlthreadCreate(&fixture.thread, "thread", Immediate, 0x27, nullptr, 0, &fixture.runtime, nullptr, 0), State, "create over a live ulthread");
}};

const Case finalizeUnjoined{"Finalize_UnjoinedUlthread_ReturnsBusyUntilJoined", [] {
    UltFixture fixture;
    RequireEqual(sceUltUlthreadRuntimeCreate(&fixture.runtime, "runtime", 1, 1, nullptr, nullptr, 0), Ok, "create the runtime");
    RequireEqual(sceUltUlthreadCreate(&fixture.thread, "thread", Immediate, 0x27, nullptr, 0, &fixture.runtime, nullptr, 0), Ok, "create the ulthread");
    RequireEqual(sceUltFinalize(), Busy, "finalize before join");
    std::int32_t status = 0;
    RequireEqual(sceUltUlthreadJoin(&fixture.thread, &status), Ok, "join");
    RequireEqual(status, std::int32_t{0x27}, "exit status");
    RequireEqual(sceUltFinalize(), Ok, "finalize after join");
}};

const Case workAreaSize{"UlthreadRuntimeGetWorkAreaSize_OneThreadOneWorker_ReturnsThreadAndStackArea", [] {
    RequireEqual(sceUltUlthreadRuntimeGetWorkAreaSize(1, 1), std::uint64_t{256u + 16u * 1024u}, "work area for 1 ulthread and 1 worker");
}};

const Case optParamInitialize{"UlthreadRuntimeOptParamInitialize_Buffer_ZeroesOrRejectsNull", [] {
    const UltFixture fixture;
    std::uint8_t optParam[128];
    std::memset(optParam, 0xff, sizeof(optParam));
    RequireEqual(_sceUltUlthreadRuntimeOptParamInitialize(nullptr, 0), Null, "null opt param");
    RequireEqual(_sceUltUlthreadRuntimeOptParamInitialize(optParam, 0), Ok, "initialize the opt param");
    RequireEqual(optParam[0], std::uint8_t{0}, "opt param zeroed");
}};

const Case tryJoin{"UlthreadTryJoin_RunningThenExited_ReturnsBusyThenStatus", [] {
    UltFixture fixture;
    RequireEqual(_sceUltUlthreadRuntimeCreate(&fixture.runtime, "runtime", 1, 1, nullptr, nullptr, 0), Ok, "create the runtime");
    RequireEqual(_sceUltUlthreadCreate(&fixture.thread, "parked", Parked, 0x3C, nullptr, 0, &fixture.runtime, nullptr, 0), Ok, "create the parked ulthread");
    WaitUntilStarted();

    std::int32_t status = 0;
    const int running = sceUltUlthreadTryJoin(&fixture.thread, &status);
    released.store(true, std::memory_order_release);
    int joined;
    while ((joined = sceUltUlthreadTryJoin(&fixture.thread, &status)) == Busy) std::this_thread::yield();

    RequireEqual(running, Busy, "try-join a running ulthread");
    RequireEqual(joined, Ok, "try-join an exited ulthread");
    RequireEqual(status, std::int32_t{0x3C}, "exit status");
    RequireEqual(sceUltUlthreadTryJoin(&fixture.thread, &status), State, "try-join an already joined ulthread");
    RequireEqual(sceUltUlthreadTryJoin(nullptr, &status), Null, "try-join a null ulthread");
    RequireEqual(sceUltUlthreadRuntimeDestroy(&fixture.runtime), Ok, "destroy the idle runtime");
}};

const Case destroyInvalid{"UlthreadRuntimeDestroy_NullOrUnregisteredRuntime_ReturnsNullOrState", [] {
    UltFixture fixture;
    RequireEqual(sceUltUlthreadRuntimeDestroy(nullptr), Null, "destroy a null runtime");
    RequireEqual(sceUltUlthreadRuntimeDestroy(&fixture.runtime), State, "destroy an unregistered runtime");
}};

const Case destroyBusy{"UlthreadRuntimeDestroy_RuntimeWithUnjoinedThread_ReturnsBusyUntilJoined", [] {
    UltFixture fixture;
    RequireEqual(_sceUltUlthreadRuntimeCreate(&fixture.runtime, "runtime", 1, 1, nullptr, nullptr, 0), Ok, "create the runtime");
    RequireEqual(_sceUltUlthreadRuntimeCreate(&fixture.other, "other", 1, 1, nullptr, nullptr, 0), Ok, "create the other runtime");
    RequireEqual(_sceUltUlthreadCreate(&fixture.thread, "thread", Immediate, 0x11, nullptr, 0, &fixture.runtime, nullptr, 0), Ok, "create the ulthread");
    RequireEqual(sceUltUlthreadRuntimeDestroy(&fixture.other), Ok, "destroy the runtime without ulthreads");
    RequireEqual(sceUltUlthreadRuntimeDestroy(&fixture.runtime), Busy, "destroy the runtime with an unjoined ulthread");
    std::int32_t status = 0;
    RequireEqual(sceUltUlthreadJoin(&fixture.thread, &status), Ok, "join");
    RequireEqual(status, std::int32_t{0x11}, "exit status");
    RequireEqual(sceUltUlthreadRuntimeDestroy(&fixture.runtime), Ok, "destroy the runtime after join");
    RequireEqual(sceUltFinalize(), Ok, "finalize");
}};

const Case destroyedRuntime{"UlthreadRuntimeDestroy_DestroyedRuntime_RejectsDestroyAndCreate", [] {
    UltFixture fixture;
    RequireEqual(_sceUltUlthreadRuntimeCreate(&fixture.runtime, "runtime", 1, 1, nullptr, nullptr, 0), Ok, "create the runtime");
    RequireEqual(sceUltUlthreadRuntimeDestroy(&fixture.runtime), Ok, "destroy the runtime");
    RequireEqual(sceUltUlthreadRuntimeDestroy(&fixture.runtime), State, "destroy a destroyed runtime");
    RequireEqual(_sceUltUlthreadCreate(&fixture.thread, "thread", Immediate, 0, nullptr, 0, &fixture.runtime, nullptr, 0), State, "create a ulthread on a destroyed runtime");
    RequireEqual(sceUltFinalize(), Ok, "finalize");
}};

} // namespace
