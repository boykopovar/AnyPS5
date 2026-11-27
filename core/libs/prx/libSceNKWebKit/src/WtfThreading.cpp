// Derived from Source/WTF/wtf/posix/ThreadingPOSIX.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2007, 2009, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood <jhaygood@reaktix.com>
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceNKWebKit/include/WtfThreadingExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WtfAbi::Mutex;
using NKWebKit::WtfAbi::ThreadCondition;

static_assert(offsetof(Mutex, mutex) == 0);
static_assert(offsetof(ThreadCondition, condition) == 0);

constexpr int PosixMutexNormal = 3;
constexpr int PosixEbusy = 16;

void Check(int result, const char* call) {
    if (result != 0)
        throw std::runtime_error(std::string("WTF threading: ") + call + " failed with " + std::to_string(result));
}

void RequireReadableGuestBytes(const void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(pointer, size);
    if (!GuestReadable_nid_no_patch(pointer, size))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void RequireAligned(const void* pointer, std::size_t alignment, const char* function) {
    if ((reinterpret_cast<std::uintptr_t>(pointer) % alignment) != 0)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template<typename T> T* RequireObject(T* self, const char* caller, const char* function, bool requireReadable) {
    if (!self)
        throw std::invalid_argument(std::string(caller) + " requires an object");
    RequireAligned(self, alignof(T), function);
    if (requireReadable)
        RequireReadableGuestBytes(self, sizeof(T), function);
    return self;
}

template<typename T>
struct WritableGuestObject {
    GuestArena::HostWrite writeGuard;

    WritableGuestObject(T* pointer, const char* caller, const char* function, bool requireReadable)
        : writeGuard(RequireObject(pointer, caller, function, requireReadable), sizeof(T)) {
        if (!writeGuard.Open() || !GuestWritable_nid_no_patch(pointer, sizeof(T)))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
};

PthreadMutex CopyInMutexHandle(Mutex* mutex, const char* caller, const char* function) {
    RequireObject(mutex, caller, function, true);
    PthreadMutex handle = nullptr;
    if (!GuestCopyIn_nid_no_patch(&handle, mutex, sizeof(handle)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return handle;
}

void Construct(Mutex* self, const char* function) {
    const WritableGuestObject<Mutex> writeGuard(self, "WTF::Mutex::Mutex", function, false);
    PthreadMutexattr attribute = nullptr;
    Check(pthread_mutexattr_init_nid_postfix(&attribute), "pthread_mutexattr_init");
    PthreadMutex handle = nullptr;
    int result = pthread_mutexattr_settype_nid_postfix(&attribute, PosixMutexNormal);
    if (result == 0)
        result = pthread_mutex_init_nid_postfix(&handle, &attribute);
    if (result == 0)
        result = pthread_mutex_setname_np_nid_postfix(&handle, "SceNKWTF");
    const int destroyed = pthread_mutexattr_destroy_nid_postfix(&attribute);
    if (result != 0) {
        if (handle) pthread_mutex_destroy_nid_postfix(&handle);
        Check(result, "pthread_mutexattr_settype, pthread_mutex_init or pthread_mutex_setname_np");
    }
    if (destroyed != 0) {
        if (handle) pthread_mutex_destroy_nid_postfix(&handle);
        Check(destroyed, "pthread_mutexattr_destroy");
    }
    if (!GuestCopyOut_nid_no_patch(self, &handle, sizeof(handle))) {
        pthread_mutex_destroy_nid_postfix(&handle);
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
}

void Destroy(Mutex* self, const char* function) {
    const WritableGuestObject<Mutex> writeGuard(self, "WTF::Mutex::~Mutex", function, true);
    Check(pthread_mutex_destroy_nid_postfix(&self->mutex), "pthread_mutex_destroy");
}

void Lock(Mutex* self, const char* function) {
    const WritableGuestObject<Mutex> writeGuard(self, "WTF::Mutex::lock", function, true);
    Check(pthread_mutex_lock_nid_postfix(&self->mutex), "pthread_mutex_lock");
}

bool TryLock(Mutex* self, const char* function) {
    const WritableGuestObject<Mutex> writeGuard(self, "WTF::Mutex::tryLock", function, true);
    const int result = pthread_mutex_trylock_nid_postfix(&self->mutex);
    if (result == 0)
        return true;
    if (result == PosixEbusy)
        return false;
    Check(result, "pthread_mutex_trylock");
    return false;
}

void Unlock(Mutex* self, const char* function) {
    PthreadMutex handle = CopyInMutexHandle(self, "WTF::Mutex::unlock", function);
    Check(pthread_mutex_unlock_nid_postfix(&handle), "pthread_mutex_unlock");
}

void DestroyCondition(ThreadCondition* self, const char* function) {
    const WritableGuestObject<ThreadCondition> writeGuard(self, "WTF::ThreadCondition::~ThreadCondition", function, true);
    pthread_cond_destroy_nid_postfix(&self->condition);
}

void Wait(ThreadCondition* self, Mutex* mutex, const char* function) {
    const WritableGuestObject<ThreadCondition> conditionGuard(self, "WTF::ThreadCondition::wait", function, true);
    PthreadMutex mutexHandle = CopyInMutexHandle(mutex, "WTF::ThreadCondition::wait", function);
    Check(pthread_cond_wait_nid_postfix(&self->condition, &mutexHandle), "pthread_cond_wait");
}

bool TimedWait(ThreadCondition* self, Mutex* mutex, WTF::WallTime absoluteTime, const char* function) {
    const WritableGuestObject<ThreadCondition> conditionGuard(self, "WTF::ThreadCondition::timedWait", function, true);
    PthreadMutex mutexHandle = CopyInMutexHandle(mutex, "WTF::ThreadCondition::timedWait", function);
    const double rawSeconds = absoluteTime.m_value;
    if (std::isnan(rawSeconds))
        throw std::invalid_argument("WTF::ThreadCondition::timedWait requires a time");
    if (std::isinf(rawSeconds)) {
        if (rawSeconds < 0)
            return false;
        Check(pthread_cond_wait_nid_postfix(&self->condition, &mutexHandle), "pthread_cond_wait");
        return true;
    }

    if (rawSeconds < _ZN3WTF8WallTime3nowEv().m_value)
        return false;

    if (rawSeconds > static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
        Check(pthread_cond_wait_nid_postfix(&self->condition, &mutexHandle), "pthread_cond_wait");
        return true;
    }

    const auto timeSeconds = static_cast<std::int64_t>(rawSeconds);
    const KernelTimespec targetTime{timeSeconds, static_cast<std::int64_t>((rawSeconds - static_cast<double>(timeSeconds)) * 1E9)};
    return pthread_cond_timedwait_nid_postfix(&self->condition, &mutexHandle, &targetTime) == 0;
}

void Signal(ThreadCondition* self, const char* function) {
    const WritableGuestObject<ThreadCondition> conditionGuard(self, "WTF::ThreadCondition::signal", function, true);
    Check(pthread_cond_signal_nid_postfix(&self->condition), "pthread_cond_signal");
}

void Broadcast(ThreadCondition* self, const char* function) {
    const WritableGuestObject<ThreadCondition> conditionGuard(self, "WTF::ThreadCondition::broadcast", function, true);
    Check(pthread_cond_broadcast_nid_postfix(&self->condition), "pthread_cond_broadcast");
}

}

extern "C" {

void APS5_VABI _ZN3WTF5MutexC1Ev(Mutex* self) {
    Construct(self, __func__);
}

void APS5_VABI _ZN3WTF5MutexC2Ev(Mutex* self) {
    Construct(self, __func__);
}

void APS5_VABI _ZN3WTF5MutexD1Ev(Mutex* self) {
    Destroy(self, __func__);
}

void APS5_VABI _ZN3WTF5MutexD2Ev(Mutex* self) {
    Destroy(self, __func__);
}

void APS5_VABI _ZN3WTF5Mutex4lockEv(Mutex* self) {
    Lock(self, __func__);
}

bool APS5_VABI _ZN3WTF5Mutex7tryLockEv(Mutex* self) {
    return TryLock(self, __func__);
}

void APS5_VABI _ZN3WTF5Mutex6unlockEv(Mutex* self) {
    Unlock(self, __func__);
}

void APS5_VABI _ZN3WTF15ThreadConditionD1Ev(ThreadCondition* self) {
    DestroyCondition(self, __func__);
}

void APS5_VABI _ZN3WTF15ThreadConditionD2Ev(ThreadCondition* self) {
    DestroyCondition(self, __func__);
}

void APS5_VABI _ZN3WTF15ThreadCondition4waitERNS_5MutexE(ThreadCondition* self, Mutex* mutex) {
    Wait(self, mutex, __func__);
}

bool APS5_VABI _ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(ThreadCondition* self, Mutex* mutex, WTF::WallTime absoluteTime) {
    return TimedWait(self, mutex, absoluteTime, __func__);
}

void APS5_VABI _ZN3WTF15ThreadCondition6signalEv(ThreadCondition* self) {
    Signal(self, __func__);
}

void APS5_VABI _ZN3WTF15ThreadCondition9broadcastEv(ThreadCondition* self) {
    Broadcast(self, __func__);
}

}
