#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
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

using Destructor = void (APS5_VABI*)(void*);

void Require(bool value) { if (!value) std::abort(); }

void* dsoHandle = &dsoHandle;
void* staleDsoHandle = nullptr;
KernelModule self = 0;
std::vector<std::intptr_t> calls;

void* Object(std::intptr_t value) { return reinterpret_cast<void*>(value); }

int Register(Destructor destructor, std::intptr_t object, void* dso = &dsoHandle) {
    return _sceLibcInternalThreadAtexit_nid_postfix(destructor, Object(object), dso);
}

void APS5_VABI Record(void* object) {
    calls.push_back(reinterpret_cast<std::intptr_t>(object));
}

void APS5_VABI RecordAndRegister(void* object) {
    Record(object);
    const auto value = reinterpret_cast<std::intptr_t>(object);
    if (value < 17)
        Require(Register(RecordAndRegister, value + 1) == 0);
}

void* APS5_VABI Worker(void*) {
    Require(Register(Record, 20) == 0);
    Require(_sceLibcInternalForceTlsDestructor_nid_postfix(self) == 0);
    Require(calls == std::vector<std::intptr_t>{20});
    return nullptr;
}

KernelModule ModuleOf(const void* address) {
    ModuleInfoEx info{};
    info.st_size = sizeof(ModuleInfoEx);
    Require(sceKernelGetModuleInfoFromAddr(reinterpret_cast<std::uintptr_t>(address), 2, &info) == 0);
    return info.id;
}

bool ForceThrows(KernelModule handle) {
    try {
        _sceLibcInternalForceTlsDestructor_nid_postfix(handle);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}

int main() {
    self = ModuleOf(&dsoHandle);
    void* otherDso = dlsym(RTLD_NEXT, "_sceLibcInternalThreadDtors_nid_postfix");
    Require(otherDso != nullptr);
    const KernelModule other = ModuleOf(otherDso);
    Require(other != self && other != 0);
    int local = 0;

    Require(_sceLibcInternalForceTlsDestructor_nid_postfix(self) == 0);
    Require(calls.empty());

    Require(Register(Record, 1) == 0);
    Require(Register(Record, 2, otherDso) == 0);
    Require(Register(Record, 3) == 0);
    Require(Register(Record, 4, &staleDsoHandle) == 0);
    Require(Register(Record, 5, &local) == 0);
    Require(_sceLibcInternalForceTlsDestructor_nid_postfix(self + 1000) == 0);
    Require(calls.empty());
    Require(_sceLibcInternalForceTlsDestructor_nid_postfix(self) == 0);
    Require(calls == std::vector<std::intptr_t>{3, 1});
    Require(_sceLibcInternalForceTlsDestructor_nid_postfix(self) == 0);
    Require(calls.size() == 2);
    Require(_sceLibcInternalForceTlsDestructor_nid_postfix(0) == 0);
    Require(calls.size() == 2);
    _sceLibcInternalThreadDtors_nid_postfix();
    Require(calls == std::vector<std::intptr_t>{3, 1, 2});

    calls.clear();
    Require(Register(Record, 6, otherDso) == 0);
    Require(_sceLibcInternalForceTlsDestructor_nid_postfix(other) == 0);
    Require(calls.empty());
    _sceLibcInternalThreadDtors_nid_postfix();
    Require(calls.empty());

    Require(Register(RecordAndRegister, 10) == 0);
    Require(_sceLibcInternalForceTlsDestructor_nid_postfix(self) == 0);
    Require(calls == std::vector<std::intptr_t>{10, 11, 12, 13});
    calls.clear();
    Require(_sceLibcInternalForceTlsDestructor_nid_postfix(self) == 0);
    Require(calls == std::vector<std::intptr_t>{14, 15, 16, 17});
    calls.clear();

    Require(Register(Record, 30) == 0);
    Pthread worker = nullptr;
    Require(scePthreadCreate(&worker, nullptr, Worker, nullptr, "ForceTlsDestructor") == 0);
    Require(scePthreadJoin(worker, nullptr) == 0);
    Require(calls == std::vector<std::intptr_t>{20});
    calls.clear();

    std::vector<unsigned char> heap(64);
    Require(Register(reinterpret_cast<Destructor>(heap.data()), 7) == 0);
    Require(ForceThrows(self));
    Require(calls.empty());
    Require(_sceLibcInternalForceTlsDestructor_nid_postfix(self) == 0);
    Require(calls == std::vector<std::intptr_t>{30});
}
