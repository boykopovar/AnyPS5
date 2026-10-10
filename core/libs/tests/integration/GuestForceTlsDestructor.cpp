#include "SceTypes.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <dlfcn.h>
#include <stdexcept>
#include <vector>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI sceKernelGetModuleInfoFromAddr(std::uint64_t address, int flags, ModuleInfoEx* info);
int APS5_VABI _sceLibcInternalThreadAtexit_nid_postfix(void (APS5_VABI* destructor)(void*), void* object, void* dsoSymbol);
void APS5_VABI _sceLibcInternalThreadDtors_nid_postfix();
int APS5_VABI _sceLibcInternalForceTlsDestructor_nid_postfix(KernelModule handle);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

using Destructor = void (APS5_VABI*)(void*);

void* dsoHandle = &dsoHandle;
void* staleDsoHandle = nullptr;
KernelModule self = 0;
std::vector<std::intptr_t> calls;
int failedCallbackRegistrations = 0;

struct WorkerOutcome {
    int registerResult = -1;
    int forceResult = -1;
    std::vector<std::intptr_t> calls;
};

WorkerOutcome workerOutcome;

void* Object(std::intptr_t value) { return reinterpret_cast<void*>(value); }

int Register(Destructor destructor, std::intptr_t object, void* dso = &dsoHandle) {
    return _sceLibcInternalThreadAtexit_nid_postfix(destructor, Object(object), dso);
}

int Force(KernelModule handle) {
    return _sceLibcInternalForceTlsDestructor_nid_postfix(handle);
}

void APS5_VABI Record(void* object) {
    calls.push_back(reinterpret_cast<std::intptr_t>(object));
}

void APS5_VABI RecordAndRegister(void* object) {
    Record(object);
    const auto value = reinterpret_cast<std::intptr_t>(object);
    if (value < 17 && Register(RecordAndRegister, value + 1) != 0) ++failedCallbackRegistrations;
}

void* APS5_VABI Worker(void*) {
    workerOutcome.registerResult = Register(Record, 20);
    workerOutcome.forceResult = Force(self);
    workerOutcome.calls = calls;
    return nullptr;
}

KernelModule ModuleOf(const void* address) {
    ModuleInfoEx info{};
    info.st_size = sizeof(ModuleInfoEx);
    RequireEqual(sceKernelGetModuleInfoFromAddr(reinterpret_cast<std::uintptr_t>(address), 2, &info), 0, "module info lookup");
    return info.id;
}

bool ForceThrows(KernelModule handle) {
    try {
        Force(handle);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

class ForceFixture {
public:
    ForceFixture() {
        Reset();
        self = ModuleOf(&dsoHandle);
        otherDso = dlsym(RTLD_NEXT, "_sceLibcInternalThreadDtors_nid_postfix");
        Require(otherDso != nullptr, "resolve a libSceLibcInternal symbol");
        other = ModuleOf(otherDso);
    }

    ~ForceFixture() {
        for (int attempt = 0; attempt < 64; ++attempt) {
            if (!DrainThrows()) break;
        }
        Reset();
        self = 0;
    }

    ForceFixture(const ForceFixture&) = delete;
    ForceFixture& operator=(const ForceFixture&) = delete;

    void* otherDso = nullptr;
    KernelModule other = 0;

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
        failedCallbackRegistrations = 0;
        workerOutcome = WorkerOutcome{};
    }
};

void RegisterMixed(const ForceFixture& fixture, int& local) {
    RequireEqual(Register(Record, 1), 0, "register 1 for this module");
    RequireEqual(Register(Record, 2, fixture.otherDso), 0, "register 2 for the other module");
    RequireEqual(Register(Record, 3), 0, "register 3 for this module");
    RequireEqual(Register(Record, 4, &staleDsoHandle), 0, "register 4 with a stale dso handle");
    RequireEqual(Register(Record, 5, &local), 0, "register 5 with a stack dso handle");
}

const Case modules{"ModuleInfo_LibcInternalSymbol_BelongsToAnotherModule", [] {
    const ForceFixture fixture;
    Require(fixture.other != self, "other module differs from this module");
    Require(fixture.other != 0, "other module handle is not zero");
}};

const Case empty{"ForceTlsDestructor_NoDestructors_ReturnsZeroAndRunsNothing", [] {
    const ForceFixture fixture;
    RequireEqual(Force(self), 0, "force result");
    Require(calls.empty(), "no destructor ran");
}};

const Case unknownModule{"ForceTlsDestructor_UnknownModule_RunsNothing", [] {
    const ForceFixture fixture;
    int local = 0;
    RegisterMixed(fixture, local);
    RequireEqual(Force(self + 1000), 0, "force result");
    Require(calls.empty(), "no destructor ran");
}};

const Case ownModule{"ForceTlsDestructor_OwnModule_RunsOwnDestructorsInReverseOrder", [] {
    const ForceFixture fixture;
    int local = 0;
    RegisterMixed(fixture, local);
    RequireEqual(Force(self), 0, "force result");
    Require(calls == std::vector<std::intptr_t>{3, 1}, "destructor order 3, 1");
}};

const Case ownModuleAgain{"ForceTlsDestructor_OwnModuleAgain_RunsNothing", [] {
    const ForceFixture fixture;
    int local = 0;
    RegisterMixed(fixture, local);
    RequireEqual(Force(self), 0, "first force result");
    RequireEqual(Force(self), 0, "second force result");
    RequireEqual(calls.size(), std::size_t {2}, "destructor calls");
}};

const Case unresolvedDso{"ForceTlsDestructor_NullModule_DropsUnresolvedDsoWithoutRunning", [] {
    const ForceFixture fixture;
    int local = 0;
    RegisterMixed(fixture, local);
    RequireEqual(Force(self), 0, "force own module");
    RequireEqual(Force(0), 0, "force null module");
    RequireEqual(calls.size(), std::size_t {2}, "destructor calls after the null module");
    _sceLibcInternalThreadDtors_nid_postfix();
    Require(calls == std::vector<std::intptr_t>{3, 1, 2}, "only the other module's destructor remained");
}};

const Case otherModule{"ForceTlsDestructor_OtherModuleWithForeignDso_DropsWithoutRunning", [] {
    const ForceFixture fixture;
    RequireEqual(Register(Record, 6, fixture.otherDso), 0, "register 6 for the other module");
    RequireEqual(Force(fixture.other), 0, "force result");
    Require(calls.empty(), "no destructor ran when forced");
    _sceLibcInternalThreadDtors_nid_postfix();
    Require(calls.empty(), "the destructor was dropped");
}};

const Case reregistering{"ForceTlsDestructor_DestructorsRegisteringMore_RunsFourPassesPerCall", [] {
    const ForceFixture fixture;
    RequireEqual(Register(RecordAndRegister, 10), 0, "register 10");
    RequireEqual(Force(self), 0, "first force result");
    Require(calls == std::vector<std::intptr_t>{10, 11, 12, 13}, "first call runs 10 to 13");
    calls.clear();
    RequireEqual(Force(self), 0, "second force result");
    Require(calls == std::vector<std::intptr_t>{14, 15, 16, 17}, "second call runs 14 to 17");
    RequireEqual(failedCallbackRegistrations, 0, "failed registrations inside destructors");
}};

const Case workerThread{"ForceTlsDestructor_OnWorkerThread_RunsOnlyWorkerDestructors", [] {
    const ForceFixture fixture;
    RequireEqual(Register(Record, 30), 0, "register 30 on the main thread");
    Pthread worker = nullptr;
    RequireEqual(scePthreadCreate(&worker, nullptr, Worker, nullptr, "ForceTlsDestructor"), 0, "create the worker");
    RequireEqual(scePthreadJoin(worker, nullptr), 0, "join the worker");
    RequireEqual(workerOutcome.registerResult, 0, "register 20 on the worker");
    RequireEqual(workerOutcome.forceResult, 0, "force on the worker");
    Require(workerOutcome.calls == std::vector<std::intptr_t>{20}, "worker saw only its destructor");
    Require(calls == std::vector<std::intptr_t>{20}, "no further destructor ran after the worker exited");
}};

const Case invalidDestructor{"ForceTlsDestructor_DestructorOutsideLoadedImage_ThrowsAndDropsIt", [] {
    std::vector<unsigned char> heap(64);
    const ForceFixture fixture;
    RequireEqual(Register(Record, 30), 0, "register 30");
    RequireEqual(Register(reinterpret_cast<Destructor>(heap.data()), 7), 0, "register a heap destructor");
    Require(ForceThrows(self), "force throws");
    Require(calls.empty(), "no destructor ran");
    RequireEqual(Force(self), 0, "second force result");
    Require(calls == std::vector<std::intptr_t>{30}, "the valid destructor runs afterwards");
}};

} // namespace
