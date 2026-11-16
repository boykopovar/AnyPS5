/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Copyright (c) 2011, The WebRTC project authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 *   * Neither the name of Google nor the names of its contributors may
 *     be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/GuestPtr.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr int PosixMutexRecursive = 2;

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

void RequireWritable(const WebRtcRecursiveCriticalSection* section, const char* caller) {
    if (!section)
        throw std::invalid_argument(std::string(caller) + " requires a critical section");
    ErrnoGuard guard;
    if (!GuestWritable_nid_no_patch(const_cast<WebRtcRecursiveCriticalSection*>(section), sizeof(*section)))
        throw std::invalid_argument(std::string(caller) + " requires a critical section");
}

PthreadMutex* MutexField(const WebRtcRecursiveCriticalSection* section) {
    return reinterpret_cast<PthreadMutex*>(
        reinterpret_cast<std::uintptr_t>(section) + offsetof(WebRtcRecursiveCriticalSection, mutex));
}

void Check(int result, const char* call) {
    if (result != 0)
        throw std::runtime_error(std::string("rtc::RecursiveCriticalSection: ") + call + " failed with " + std::to_string(result));
}

void Enter(const WebRtcRecursiveCriticalSection* section, const char* caller) {
    RequireWritable(section, caller);
    Check(pthread_mutex_lock_nid_postfix(MutexField(section)), "pthread_mutex_lock");
}

void Leave(const WebRtcRecursiveCriticalSection* section, const char* caller) {
    RequireWritable(section, caller);
    Check(pthread_mutex_unlock_nid_postfix(MutexField(section)), "pthread_mutex_unlock");
}

}

extern "C" {

void APS5_VABI _ZN3rtc24RecursiveCriticalSectionC1Ev_nid_postfix(WebRtcRecursiveCriticalSection* section) {
    RequireWritable(section, "RecursiveCriticalSection::RecursiveCriticalSection");
    PthreadMutexattr attribute = nullptr;
    Check(pthread_mutexattr_init_nid_postfix(&attribute), "pthread_mutexattr_init");
    int result = pthread_mutexattr_settype_nid_postfix(&attribute, PosixMutexRecursive);
    if (result == 0)
        result = pthread_mutex_init_nid_postfix(MutexField(section), &attribute);
    const int destroyed = pthread_mutexattr_destroy_nid_postfix(&attribute);
    Check(result, "pthread_mutexattr_settype or pthread_mutex_init");
    Check(destroyed, "pthread_mutexattr_destroy");
}

void APS5_VABI _ZN3rtc24RecursiveCriticalSectionD1Ev_nid_postfix(WebRtcRecursiveCriticalSection* section) {
    RequireWritable(section, "RecursiveCriticalSection::~RecursiveCriticalSection");
    Check(pthread_mutex_destroy_nid_postfix(MutexField(section)), "pthread_mutex_destroy");
}

void APS5_VABI _ZNK3rtc24RecursiveCriticalSection5EnterEv_nid_postfix(const WebRtcRecursiveCriticalSection* section) {
    Enter(section, "RecursiveCriticalSection::Enter");
}

void APS5_VABI _ZNK3rtc24RecursiveCriticalSection5LeaveEv_nid_postfix(const WebRtcRecursiveCriticalSection* section) {
    Leave(section, "RecursiveCriticalSection::Leave");
}

void APS5_VABI _ZN3rtc9CritScopeC1EPKNS_24RecursiveCriticalSectionE_nid_postfix(WebRtcCritScope* scope, const WebRtcRecursiveCriticalSection* section) {
    if (!scope)
        throw std::invalid_argument("CritScope::CritScope requires a scope");
    RequireWritable(section, "CritScope::CritScope");
    {
        ErrnoGuard guard;
        if (!GuestPtr<WebRtcCritScope>(scope).CopyOut(WebRtcCritScope{section}))
            throw std::invalid_argument("CritScope::CritScope requires a scope");
    }
    Check(pthread_mutex_lock_nid_postfix(MutexField(section)), "pthread_mutex_lock");
}

void APS5_VABI _ZN3rtc9CritScopeD1Ev_nid_postfix(WebRtcCritScope* scope) {
    if (!scope)
        throw std::invalid_argument("CritScope::~CritScope requires a scope");
    WebRtcCritScope hostScope{};
    {
        ErrnoGuard guard;
        if (!GuestPtr<const WebRtcCritScope>(scope).CopyIn(hostScope))
            throw std::invalid_argument("CritScope::~CritScope requires a scope");
    }
    Leave(hostScope.section, "CritScope::~CritScope");
}

}
