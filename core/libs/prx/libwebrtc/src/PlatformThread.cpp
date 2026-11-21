/* Derived from rtc_base/platform_thread.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
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
#include "prx/libc/include/Api.hpp"

#include <stdexcept>

namespace {

WebRtcPlatformThread& Checked(WebRtcPlatformThread* thread, const char* function) {
    if (!thread)
        throw std::invalid_argument(function);
    return *thread;
}

void Finalize(WebRtcPlatformThread& thread) {
    if (!thread.hasHandle)
        return;
    if (thread.joinable && _Thrd_join_nid_postfix(thread.handle, nullptr) != 0)
        throw std::runtime_error("PlatformThread::Finalize: pthread_join failed");
    thread.hasHandle = false;
}

}

extern "C" {

void APS5_VABI _ZN3rtc14PlatformThread8FinalizeEv_nid_postfix(WebRtcPlatformThread* thread) {
    Finalize(Checked(thread, "PlatformThread::Finalize requires a thread"));
}

void APS5_VABI _ZN3rtc14PlatformThreadD1Ev_nid_postfix(WebRtcPlatformThread* thread) {
    Finalize(Checked(thread, "PlatformThread::~PlatformThread requires a thread"));
}

WebRtcPlatformThread* APS5_VABI _ZN3rtc14PlatformThreadaSEOS0__nid_postfix(WebRtcPlatformThread* thread, WebRtcPlatformThread* other) {
    WebRtcPlatformThread& target = Checked(thread, "PlatformThread::operator= requires a thread");
    WebRtcPlatformThread& source = Checked(other, "PlatformThread::operator= requires a source");
    Finalize(target);
    target.handle = source.handle;
    target.hasHandle = source.hasHandle;
    target.joinable = source.joinable;
    source.hasHandle = false;
    return &target;
}

}
