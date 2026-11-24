/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
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
#include "prx/libc/libc.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"

#include <cerrno>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

constexpr int GuestClockMonotonic = 4;
constexpr std::int64_t MicrosecondsPerSecond = 1000000;
constexpr std::int64_t NanosecondsPerMicrosecond = 1000;
constexpr std::int64_t NanosecondsPerSecond = 1000000000;
constexpr std::int64_t PlusInfinityMicroseconds = std::numeric_limits<std::int64_t>::max();

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

void RequireServer(const WebRtcNullSocketServer* server, const char* caller) {
    if (!server)
        throw std::invalid_argument(std::string(caller) + " requires a socket server");
}

void RequireWritable(const WebRtcNullSocketServer* server, const char* caller) {
    RequireServer(server, caller);
    ErrnoGuard guard;
    if (!GuestWritable_nid_no_patch(const_cast<WebRtcNullSocketServer*>(server), sizeof(*server)))
        throw std::invalid_argument(std::string(caller) + " requires a writable socket server");
}

void Check(int result, const char* call) {
    if (result != 0)
        throw std::runtime_error(std::string("rtc::NullSocketServer: ") + call + " failed with " + std::to_string(result));
}

void InitializeGuestEvent(WebRtcEvent* event) {
    event->isManualReset = false;
    event->eventStatus = false;
    Check(pthread_mutex_init_nid_postfix(&event->eventMutex, nullptr), "pthread_mutex_init");
    PthreadCondattr attribute = nullptr;
    Check(pthread_condattr_init_nid_postfix(&attribute), "pthread_condattr_init");
    int result = pthread_condattr_setclock_nid_postfix(&attribute, GuestClockMonotonic);
    if (result == 0)
        result = pthread_cond_init_nid_postfix(&event->eventCond, &attribute);
    const int destroyed = pthread_condattr_destroy_nid_postfix(&attribute);
    Check(result, "pthread_condattr_setclock or pthread_cond_init");
    Check(destroyed, "pthread_condattr_destroy");
}

void DestroyGuestEvent(WebRtcEvent* event) {
    const int mutexDestroyed = pthread_mutex_destroy_nid_postfix(&event->eventMutex);
    const int condDestroyed = pthread_cond_destroy_nid_postfix(&event->eventCond);
    Check(mutexDestroyed, "pthread_mutex_destroy");
    Check(condDestroyed, "pthread_cond_destroy");
}

void SetGuestEvent(WebRtcEvent* event) {
    Check(pthread_mutex_lock_nid_postfix(&event->eventMutex), "pthread_mutex_lock");
    event->eventStatus = true;
    const int broadcast = pthread_cond_broadcast_nid_postfix(&event->eventCond);
    const int unlocked = pthread_mutex_unlock_nid_postfix(&event->eventMutex);
    Check(broadcast, "pthread_cond_broadcast");
    Check(unlocked, "pthread_mutex_unlock");
}

KernelTimespec GetTimespec(std::int64_t microsecondsFromNow) {
    KernelTimespec deadline{};
    Check(clock_gettime_nid_postfix(GuestClockMonotonic, &deadline), "clock_gettime(CLOCK_MONOTONIC)");
    deadline.tv_sec += microsecondsFromNow / MicrosecondsPerSecond;
    deadline.tv_nsec += (microsecondsFromNow % MicrosecondsPerSecond) * NanosecondsPerMicrosecond;
    if (deadline.tv_nsec >= NanosecondsPerSecond) {
        deadline.tv_sec++;
        deadline.tv_nsec -= NanosecondsPerSecond;
    }
    return deadline;
}

void WaitGuestEvent(WebRtcEvent* event, std::int64_t giveUpAfterMicroseconds) {
    const bool forever = giveUpAfterMicroseconds == PlusInfinityMicroseconds;
    KernelTimespec giveUp{};
    if (!forever)
        giveUp = GetTimespec(giveUpAfterMicroseconds);
    Check(pthread_mutex_lock_nid_postfix(&event->eventMutex), "pthread_mutex_lock");
    int error = 0;
    while (!event->eventStatus && error == 0) {
        if (forever)
            error = pthread_cond_wait_nid_postfix(&event->eventCond, &event->eventMutex);
        else
            error = pthread_cond_timedwait_nid_postfix(&event->eventCond, &event->eventMutex, &giveUp);
    }
    if (error == 0 && !event->isManualReset)
        event->eventStatus = false;
    Check(pthread_mutex_unlock_nid_postfix(&event->eventMutex), "pthread_mutex_unlock");
}

void APS5_VABI DestroyNullSocketServer_nid_no_patch(WebRtcNullSocketServer* server);

void APS5_VABI DeleteNullSocketServer_nid_no_patch(WebRtcNullSocketServer* server) {
    DestroyNullSocketServer_nid_no_patch(server);
    _ZdlPv_nid_postfix(server);
}

void* APS5_VABI NullSocketServerCreateSocket_nid_no_patch(WebRtcNullSocketServer* server, int, int) {
    RequireServer(server, "NullSocketServer::CreateSocket");
    return nullptr;
}

void APS5_VABI NullSocketServerSetMessageQueue_nid_no_patch(WebRtcNullSocketServer* server, void*) {
    RequireServer(server, "NullSocketServer::SetMessageQueue");
}

bool APS5_VABI NullSocketServerWait_nid_no_patch(WebRtcNullSocketServer* server, std::int64_t maxWaitDurationMicroseconds, bool) {
    RequireServer(server, "NullSocketServer::Wait");
    WaitGuestEvent(&server->event, maxWaitDurationMicroseconds);
    return true;
}

void APS5_VABI NullSocketServerWakeUp_nid_no_patch(WebRtcNullSocketServer* server) {
    RequireServer(server, "NullSocketServer::WakeUp");
    SetGuestEvent(&server->event);
}

void* const Vtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&DestroyNullSocketServer_nid_no_patch),
    reinterpret_cast<void*>(&DeleteNullSocketServer_nid_no_patch),
    reinterpret_cast<void*>(&NullSocketServerCreateSocket_nid_no_patch),
    reinterpret_cast<void*>(&NullSocketServerSetMessageQueue_nid_no_patch),
    reinterpret_cast<void*>(&NullSocketServerWait_nid_no_patch),
    reinterpret_cast<void*>(&NullSocketServerWakeUp_nid_no_patch),
};

void APS5_VABI DestroyNullSocketServer_nid_no_patch(WebRtcNullSocketServer* server) {
    RequireWritable(server, "NullSocketServer::~NullSocketServer");
    server->vtable = Vtable + 2;
    DestroyGuestEvent(&server->event);
}

}

extern "C" {

void APS5_VABI _ZN3rtc16NullSocketServerC1Ev_nid_postfix(WebRtcNullSocketServer* server) {
    RequireWritable(server, "NullSocketServer::NullSocketServer");
    server->vtable = Vtable + 2;
    server->networkBinder = nullptr;
    InitializeGuestEvent(&server->event);
}

}
