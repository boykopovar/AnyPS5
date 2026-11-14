#include "prx/libSceLibcInternal/include/MutexObject.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <stdexcept>

namespace {

PthreadMutex* ReadObject_nid_no_patch(const void* self) {
    PthreadMutex* storage = nullptr;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&storage, self, sizeof(storage));
    errno = savedErrno;
    if (!copied)
        throw std::invalid_argument("std::_Mutex: unreadable object");
    return storage;
}

void CheckStorage_nid_no_patch(PthreadMutex* storage) {
    PthreadMutex handle = nullptr;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&handle, storage, sizeof(handle));
    errno = savedErrno;
    if (!copied)
        throw std::invalid_argument("std::_Mutex: unreadable mutex storage");
}

void Construct_nid_no_patch(void* self) {
    auto* storage = static_cast<PthreadMutex*>(_Znwm_nid_postfix(sizeof(PthreadMutex)));
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(self, &storage, sizeof(storage));
    errno = savedErrno;
    if (!copied) {
        _ZdlPv_nid_postfix(storage);
        throw std::invalid_argument("std::_Mutex: unwritable object");
    }
    _Mtxinit_nid_postfix(storage, "C++");
}

void DestroyStorage_nid_no_patch(void* self, PthreadMutex* storage) noexcept {
    try {
        _Mtxdst_nid_postfix(storage);
        storage = ReadObject_nid_no_patch(self);
        if (storage != nullptr)
            _ZdlPv_nid_postfix(storage);
    } catch (...) {
        _ZSt9terminatev_nid_postfix();
    }
}

void Destroy_nid_no_patch(void* self) {
    PthreadMutex* storage = ReadObject_nid_no_patch(self);
    CheckStorage_nid_no_patch(storage);
    DestroyStorage_nid_no_patch(self, storage);
}

}

extern "C" {

void APS5_VABI _ZNSt6_MutexC1Ev_nid_postfix(void* self) {
    Construct_nid_no_patch(self);
}

void APS5_VABI _ZNSt6_MutexC2Ev_nid_postfix(void* self) {
    Construct_nid_no_patch(self);
}

void APS5_VABI _ZNSt6_MutexD1Ev_nid_postfix(void* self) {
    Destroy_nid_no_patch(self);
}

void APS5_VABI _ZNSt6_MutexD2Ev_nid_postfix(void* self) {
    Destroy_nid_no_patch(self);
}

void APS5_VABI _ZNSt6_Mutex5_LockEv_nid_postfix(void* self) {
    PthreadMutex* storage = ReadObject_nid_no_patch(self);
    CheckStorage_nid_no_patch(storage);
    _Mtxlock_nid_postfix(storage);
}

void APS5_VABI _ZNSt6_Mutex7_UnlockEv_nid_postfix(void* self) {
    PthreadMutex* storage = ReadObject_nid_no_patch(self);
    CheckStorage_nid_no_patch(storage);
    _Mtxunlock_nid_postfix(storage);
}

}
