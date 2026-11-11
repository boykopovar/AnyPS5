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

#include <algorithm>
#include <atomic>
#include <mutex>
#include <stdexcept>

namespace {

std::atomic<std::int32_t> g_minSeverity{WebRtcLsNone};
std::atomic<std::int32_t> g_debugSeverity{WebRtcLsNone};
std::atomic<bool> g_logToStderr{true};
std::atomic<bool> g_logThread{false};
std::atomic<bool> g_logTimestamp{false};
std::atomic<bool> g_streamsEmpty{true};
WebRtcLogSink* g_streams = nullptr;

std::mutex& LoggingLock() {
    static std::mutex lock;
    return lock;
}

void UpdateMinLogSeverity() {
    std::int32_t minSeverity = g_debugSeverity.load();
    for (WebRtcLogSink* entry = g_streams; entry != nullptr; entry = entry->next)
        minSeverity = std::min(minSeverity, entry->minSeverity);
    g_minSeverity.store(minSeverity);
}

}

extern "C" {

int APS5_VABI _ZN3rtc10LogMessage17GetMinLogSeverityEv_nid_postfix(void) {
    return g_minSeverity.load();
}

std::int32_t APS5_VABI _ZN3rtc10LogMessage13GetLogToDebugEv_nid_postfix(void) {
    return g_debugSeverity.load();
}

void APS5_VABI _ZN3rtc10LogMessage10LogThreadsEb_nid_postfix(bool on) {
    g_logThread.store(on);
}

void APS5_VABI _ZN3rtc10LogMessage13LogTimestampsEb_nid_postfix(bool on) {
    g_logTimestamp.store(on);
}

void APS5_VABI _ZN3rtc10LogMessage10LogToDebugENS_15LoggingSeverityE_nid_postfix(std::int32_t minSeverity) {
    g_debugSeverity.store(minSeverity);
    std::lock_guard lock(LoggingLock());
    UpdateMinLogSeverity();
}

void APS5_VABI _ZN3rtc10LogMessage14SetLogToStderrEb_nid_postfix(bool logToStderr) {
    g_logToStderr.store(logToStderr);
}

void APS5_VABI _ZN3rtc10LogMessage14AddLogToStreamEPNS_7LogSinkENS_15LoggingSeverityE_nid_postfix(WebRtcLogSink* stream, std::int32_t minSeverity) {
    if (!stream)
        throw std::invalid_argument("LogMessage::AddLogToStream requires a sink");
    std::lock_guard lock(LoggingLock());
    for (WebRtcLogSink* entry = g_streams; entry != nullptr; entry = entry->next) {
        if (entry == stream)
            throw std::invalid_argument("LogMessage::AddLogToStream sink is already registered");
    }
    stream->minSeverity = minSeverity;
    stream->next = g_streams;
    g_streams = stream;
    g_streamsEmpty.store(false);
    UpdateMinLogSeverity();
}

void APS5_VABI _ZN3rtc10LogMessage17RemoveLogToStreamEPNS_7LogSinkE_nid_postfix(WebRtcLogSink* stream) {
    std::lock_guard lock(LoggingLock());
    for (WebRtcLogSink** entry = &g_streams; *entry != nullptr; entry = &(*entry)->next) {
        if (*entry == stream) {
            *entry = (*entry)->next;
            break;
        }
    }
    g_streamsEmpty.store(g_streams == nullptr);
    UpdateMinLogSeverity();
}

bool APS5_VABI _ZN3rtc10LogMessage6IsNoopENS_15LoggingSeverityE_nid_postfix(std::int32_t severity) {
    if (severity >= g_debugSeverity.load() || severity >= g_minSeverity.load())
        return false;
    return g_streamsEmpty.load();
}

}
