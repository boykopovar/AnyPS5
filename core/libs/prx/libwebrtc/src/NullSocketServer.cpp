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
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

constexpr int GuestEtimedout = 60;

struct ErrnoGuard {
    int saved = errno;
    ~ErrnoGuard() { errno = saved; }
};

void RequireWritableServer(WebRtcNullSocketServer* server, const char* caller) {
    if (!server)
        throw std::invalid_argument(std::string(caller) + " requires a NullSocketServer");
    const ErrnoGuard guard;
    if (!GuestWritable_nid_no_patch(server, sizeof(*server)))
        throw std::invalid_argument(std::string(caller) + " requires a writable NullSocketServer");
}

void Check(int result, const char* call) {
    if (result != 0)
        throw std::runtime_error(std::string("rtc::NullSocketServer: ") + call + " failed with " + std::to_string(result));
}

void APS5_VABI NullSocketServerDestroy_nid_no_patch(WebRtcNullSocketServer* server) {
    RequireWritableServer(server, "rtc::NullSocketServer::~NullSocketServer");
    WebRtcNullSocketServer local{};
    if (!GuestCopyIn_nid_no_patch(&local, server, sizeof(local)))
        throw std::invalid_argument("rtc::NullSocketServer::~NullSocketServer requires a readable NullSocketServer");
    if (local.eventCond) {
        Check(pthread_cond_destroy_nid_postfix(&local.eventCond), "pthread_cond_destroy");
        local.eventCond = nullptr;
    }
    if (local.eventMutex) {
        Check(pthread_mutex_destroy_nid_postfix(&local.eventMutex), "pthread_mutex_destroy");
        local.eventMutex = nullptr;
    }
    local.eventStatus = false;
    if (!GuestCopyOut_nid_no_patch(server, &local, sizeof(local)))
        throw std::invalid_argument("rtc::NullSocketServer::~NullSocketServer requires a writable NullSocketServer");
}

void APS5_VABI NullSocketServerDelete_nid_no_patch(WebRtcNullSocketServer* server) {
    NullSocketServerDestroy_nid_no_patch(server);
    _ZdlPv_nid_postfix(server);
}

void* APS5_VABI NullSocketServerCreateSocket_nid_no_patch(WebRtcNullSocketServer* server, int, int) {
    if (!server)
        throw std::invalid_argument("rtc::NullSocketServer::CreateSocket requires a NullSocketServer");
    return nullptr;
}

void APS5_VABI NullSocketServerSetMessageQueue_nid_no_patch(WebRtcNullSocketServer* server, void*) {
    if (!server)
        throw std::invalid_argument("rtc::NullSocketServer::SetMessageQueue requires a NullSocketServer");
}

bool APS5_VABI NullSocketServerWait_nid_no_patch(WebRtcNullSocketServer* server, std::int64_t maxWaitUs, bool) {
    if (!server)
        throw std::invalid_argument("rtc::NullSocketServer::Wait requires a NullSocketServer");
    Check(pthread_mutex_lock_nid_postfix(&server->eventMutex), "pthread_mutex_lock");
    try {
        if (maxWaitUs > 0) {
            if (maxWaitUs == std::numeric_limits<std::int64_t>::max()) {
                while (!server->eventStatus)
                    Check(pthread_cond_wait_nid_postfix(&server->eventCond, &server->eventMutex), "pthread_cond_wait");
            } else {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::microseconds(maxWaitUs);
                while (!server->eventStatus) {
                    const auto now = std::chrono::steady_clock::now();
                    if (now >= deadline)
                        break;
                    const auto remainingUs = std::chrono::duration_cast<std::chrono::microseconds>(deadline - now).count();
                    const auto clampedUs = std::max<std::int64_t>(1, remainingUs);
                    const auto stepUs = static_cast<KernelUseconds>(
                        std::min<std::int64_t>(clampedUs, std::numeric_limits<KernelUseconds>::max()));
                    const int result = pthread_cond_reltimedwait_np_nid_postfix(&server->eventCond, &server->eventMutex, stepUs);
                    if (result == GuestEtimedout) {
                        if (std::chrono::steady_clock::now() >= deadline)
                            break;
                        continue;
                    }
                    Check(result, "pthread_cond_reltimedwait_np");
                }
            }
        }
        if (server->eventStatus && !server->isManualReset)
            server->eventStatus = false;
    } catch (...) {
        pthread_mutex_unlock_nid_postfix(&server->eventMutex);
        throw;
    }
    Check(pthread_mutex_unlock_nid_postfix(&server->eventMutex), "pthread_mutex_unlock");
    return true;
}

void APS5_VABI NullSocketServerWakeUp_nid_no_patch(WebRtcNullSocketServer* server) {
    if (!server)
        throw std::invalid_argument("rtc::NullSocketServer::WakeUp requires a NullSocketServer");
    Check(pthread_mutex_lock_nid_postfix(&server->eventMutex), "pthread_mutex_lock");
    server->eventStatus = true;
    const int broadcastResult = pthread_cond_broadcast_nid_postfix(&server->eventCond);
    const int unlockResult = pthread_mutex_unlock_nid_postfix(&server->eventMutex);
    Check(broadcastResult, "pthread_cond_broadcast");
    Check(unlockResult, "pthread_mutex_unlock");
}

void* const Vtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<void*>(&NullSocketServerDestroy_nid_no_patch),
    reinterpret_cast<void*>(&NullSocketServerDelete_nid_no_patch),
    reinterpret_cast<void*>(&NullSocketServerCreateSocket_nid_no_patch),
    reinterpret_cast<void*>(&NullSocketServerSetMessageQueue_nid_no_patch),
    reinterpret_cast<void*>(&NullSocketServerWait_nid_no_patch),
    reinterpret_cast<void*>(&NullSocketServerWakeUp_nid_no_patch),
};

}

extern "C" {

void APS5_VABI _ZN3rtc16NullSocketServerC1Ev_nid_postfix(WebRtcNullSocketServer* server) {
    RequireWritableServer(server, "rtc::NullSocketServer::NullSocketServer");
    WebRtcNullSocketServer local{};
    local.vtable = Vtable + 2;
    local.networkBinder = nullptr;
    local.eventMutex = nullptr;
    local.eventCond = nullptr;
    local.isManualReset = false;
    local.eventStatus = false;
    Check(pthread_mutex_init_nid_postfix(&local.eventMutex, nullptr), "pthread_mutex_init");
    const int condResult = pthread_cond_init_nid_postfix(&local.eventCond, nullptr);
    if (condResult != 0) {
        pthread_mutex_destroy_nid_postfix(&local.eventMutex);
        Check(condResult, "pthread_cond_init");
    }
    if (!GuestCopyOut_nid_no_patch(server, &local, sizeof(local))) {
        pthread_cond_destroy_nid_postfix(&local.eventCond);
        pthread_mutex_destroy_nid_postfix(&local.eventMutex);
        throw std::invalid_argument("rtc::NullSocketServer::NullSocketServer requires a writable NullSocketServer");
    }
}

}
