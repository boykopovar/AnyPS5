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

#include "prx/libSceNKWebKit/include/WtfThreadingExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WtfAbi::Mutex;
using NKWebKit::WtfAbi::ThreadCondition;

constexpr int PosixMutexNormal = 3;
constexpr int PosixEbusy = 16;

void Check(int result, const char* call) {
    if (result != 0)
        throw std::runtime_error(std::string("WTF threading: ") + call + " failed with " + std::to_string(result));
}

template<typename T> T* Require(T* self, const char* caller) {
    if (!self)
        throw std::invalid_argument(std::string(caller) + " requires an object");
    return self;
}

void Construct(Mutex* self) {
    Require(self, "WTF::Mutex::Mutex");
    PthreadMutexattr attribute = nullptr;
    Check(pthread_mutexattr_init_nid_postfix(&attribute), "pthread_mutexattr_init");
    int result = pthread_mutexattr_settype_nid_postfix(&attribute, PosixMutexNormal);
    if (result == 0)
        result = pthread_mutex_init_nid_postfix(&self->mutex, &attribute);
    if (result == 0)
        result = pthread_mutex_setname_np_nid_postfix(&self->mutex, "SceNKWTF");
    const int destroyed = pthread_mutexattr_destroy_nid_postfix(&attribute);
    Check(result, "pthread_mutexattr_settype, pthread_mutex_init or pthread_mutex_setname_np");
    Check(destroyed, "pthread_mutexattr_destroy");
}

void Destroy(Mutex* self) {
    Check(pthread_mutex_destroy_nid_postfix(&Require(self, "WTF::Mutex::~Mutex")->mutex), "pthread_mutex_destroy");
}

void DestroyCondition(ThreadCondition* self) {
    pthread_cond_destroy_nid_postfix(&Require(self, "WTF::ThreadCondition::~ThreadCondition")->condition);
}

void Wait(ThreadCondition* self, Mutex* mutex) {
    Require(self, "WTF::ThreadCondition::wait");
    Require(mutex, "WTF::ThreadCondition::wait");
    Check(pthread_cond_wait_nid_postfix(&self->condition, &mutex->mutex), "pthread_cond_wait");
}

}

extern "C" {

void APS5_VABI _ZN3WTF5MutexC1Ev(Mutex* self) {
    Construct(self);
}

void APS5_VABI _ZN3WTF5MutexC2Ev(Mutex* self) {
    Construct(self);
}

void APS5_VABI _ZN3WTF5MutexD1Ev(Mutex* self) {
    Destroy(self);
}

void APS5_VABI _ZN3WTF5MutexD2Ev(Mutex* self) {
    Destroy(self);
}

void APS5_VABI _ZN3WTF5Mutex4lockEv(Mutex* self) {
    Check(pthread_mutex_lock_nid_postfix(&Require(self, "WTF::Mutex::lock")->mutex), "pthread_mutex_lock");
}

bool APS5_VABI _ZN3WTF5Mutex7tryLockEv(Mutex* self) {
    const int result = pthread_mutex_trylock_nid_postfix(&Require(self, "WTF::Mutex::tryLock")->mutex);
    if (result == 0)
        return true;
    if (result == PosixEbusy)
        return false;
    Check(result, "pthread_mutex_trylock");
    return false;
}

void APS5_VABI _ZN3WTF5Mutex6unlockEv(Mutex* self) {
    Check(pthread_mutex_unlock_nid_postfix(&Require(self, "WTF::Mutex::unlock")->mutex), "pthread_mutex_unlock");
}

void APS5_VABI _ZN3WTF15ThreadConditionD1Ev(ThreadCondition* self) {
    DestroyCondition(self);
}

void APS5_VABI _ZN3WTF15ThreadConditionD2Ev(ThreadCondition* self) {
    DestroyCondition(self);
}

void APS5_VABI _ZN3WTF15ThreadCondition4waitERNS_5MutexE(ThreadCondition* self, Mutex* mutex) {
    Wait(self, mutex);
}

bool APS5_VABI _ZN3WTF15ThreadCondition9timedWaitERNS_5MutexENS_8WallTimeE(ThreadCondition* self, Mutex* mutex, WTF::WallTime absoluteTime) {
    Require(self, "WTF::ThreadCondition::timedWait");
    Require(mutex, "WTF::ThreadCondition::timedWait");
    const double rawSeconds = absoluteTime.m_value;
    if (std::isnan(rawSeconds))
        throw std::invalid_argument("WTF::ThreadCondition::timedWait requires a time");
    if (std::isinf(rawSeconds)) {
        if (rawSeconds < 0)
            return false;
        Wait(self, mutex);
        return true;
    }

    if (rawSeconds < _ZN3WTF8WallTime3nowEv().m_value)
        return false;

    if (rawSeconds > static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
        Wait(self, mutex);
        return true;
    }

    const auto timeSeconds = static_cast<std::int64_t>(rawSeconds);
    const KernelTimespec targetTime{timeSeconds, static_cast<std::int64_t>((rawSeconds - static_cast<double>(timeSeconds)) * 1E9)};
    return pthread_cond_timedwait_nid_postfix(&self->condition, &mutex->mutex, &targetTime) == 0;
}

void APS5_VABI _ZN3WTF15ThreadCondition6signalEv(ThreadCondition* self) {
    Check(pthread_cond_signal_nid_postfix(&Require(self, "WTF::ThreadCondition::signal")->condition), "pthread_cond_signal");
}

void APS5_VABI _ZN3WTF15ThreadCondition9broadcastEv(ThreadCondition* self) {
    Check(pthread_cond_broadcast_nid_postfix(&Require(self, "WTF::ThreadCondition::broadcast")->condition), "pthread_cond_broadcast");
}

}
