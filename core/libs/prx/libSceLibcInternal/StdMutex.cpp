#include "prx/libSceLibcInternal/include/StdMutex.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <stdexcept>
#include <string>

namespace {

PthreadMutex ReadMutexHandle(const void* self, const char* caller) {
    if (self == nullptr)
        throw std::invalid_argument(std::string(caller) + ": null std::_Mutex");
    PthreadMutex handle = nullptr;
    if (!GuestCopyIn_nid_no_patch(&handle, self, sizeof(handle)))
        throw std::invalid_argument(std::string(caller) + ": unreadable std::_Mutex");
    return handle;
}

void WriteMutexHandle(void* self, PthreadMutex handle, const char* caller) {
    if (self == nullptr)
        throw std::invalid_argument(std::string(caller) + ": null std::_Mutex");
    if (!GuestCopyOut_nid_no_patch(self, &handle, sizeof(handle)))
        throw std::invalid_argument(std::string(caller) + ": unwritable std::_Mutex");
}

void ConstructStdMutex(void* self, const char* caller) {
    if (self == nullptr)
        throw std::invalid_argument(std::string(caller) + ": null std::_Mutex");
    PthreadMutex handle = nullptr;
    if (_Mtxinit_nid_postfix(&handle, nullptr) != 0)
        throw std::runtime_error(std::string(caller) + ": _Mtxinit failed");
    WriteMutexHandle(self, handle, caller);
}

}

extern "C" {

void APS5_VABI _ZNSt6_MutexC1Ev_nid_postfix(void* self) {
    ConstructStdMutex(self, "std::_Mutex::_Mutex(C1)");
}

void APS5_VABI _ZNSt6_MutexC2Ev_nid_postfix(void* self) {
    ConstructStdMutex(self, "std::_Mutex::_Mutex(C2)");
}
APS5_EXPORT("log6zy2C9iQ", _ZNSt6_MutexC2Ev_nid_postfix);

void APS5_VABI _ZNSt6_MutexD2Ev_nid_postfix(void* self) {
    PthreadMutex handle = ReadMutexHandle(self, "std::_Mutex::~_Mutex");
    _Mtxdst_nid_postfix(&handle);
    WriteMutexHandle(self, nullptr, "std::_Mutex::~_Mutex");
}
APS5_EXPORT("j7e7EQBD6ZA", _ZNSt6_MutexD2Ev_nid_postfix);

void APS5_VABI _ZNSt6_Mutex5_LockEv_nid_postfix(void* self) {
    PthreadMutex handle = ReadMutexHandle(self, "std::_Mutex::_Lock");
    _Mtxlock_nid_postfix(&handle);
}
APS5_EXPORT("U54NBtdj6UY", _ZNSt6_Mutex5_LockEv_nid_postfix);

void APS5_VABI _ZNSt6_Mutex7_UnlockEv_nid_postfix(void* self) {
    PthreadMutex handle = ReadMutexHandle(self, "std::_Mutex::_Unlock");
    _Mtxunlock_nid_postfix(&handle);
}
APS5_EXPORT("7OCTkL2oWyg", _ZNSt6_Mutex7_UnlockEv_nid_postfix);

}
