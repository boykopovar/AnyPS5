#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
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

using Destructor = void (APS5_VABI*)(void*);

void Require(bool value) { if (!value) std::abort(); }

int dsoHandle = 0;
std::vector<std::intptr_t> calls;
std::vector<Pthread> callers;

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
    Require(Register(Record, 9) == 0);
}

void* APS5_VABI Worker(void*) {
    Require(Register(Record, 4) == 0);
    Require(Register(Record, 5) == 0);
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

}

int main() {
    const Pthread mainThread = scePthreadSelf();
    Require(Register(Record, 1) == 0);
    Require(Register(RecordAndRegister, 2) == 0);
    Require(Register(Record, 3) == 0);
    _sceLibcInternalThreadDtors_nid_postfix();
    Require(calls == std::vector<std::intptr_t>{3, 2, 9, 1});
    Require(callers == std::vector<Pthread>(4, mainThread));
    _sceLibcInternalThreadDtors_nid_postfix();
    Require(calls.size() == 4);

    calls.clear();
    callers.clear();
    Pthread worker = nullptr;
    Require(scePthreadCreate(&worker, nullptr, Worker, nullptr, "ThreadAtexit") == 0);
    Require(scePthreadJoin(worker, nullptr) == 0);
    Require(calls == std::vector<std::intptr_t>{5, 4});
    Require(callers == std::vector<Pthread>(2, worker));

    calls.clear();
    std::vector<unsigned char> heap(64);
    Require(Register(Record, 6) == 0);
    Require(Register(nullptr, 7) == 0);
    Require(Register(reinterpret_cast<Destructor>(heap.data()), 8) == 0);
    Require(DestructorsThrow());
    Require(DestructorsThrow());
    Require(calls.empty());
    _sceLibcInternalThreadDtors_nid_postfix();
    Require(calls == std::vector<std::intptr_t>{6});
}
