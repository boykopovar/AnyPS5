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
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <stdexcept>

namespace {

WebRtcThreadManager* CreateThreadManager() {
    auto* manager = static_cast<WebRtcThreadManager*>(_Znwm_nid_postfix(sizeof(WebRtcThreadManager)));
    manager->key = 0;
    if (pthread_key_create_nid_postfix(&manager->key, nullptr) != 0)
        throw std::runtime_error("rtc::ThreadManager: pthread_key_create failed");
    return manager;
}

}

extern "C" {

WebRtcThreadManager* APS5_VABI _ZN3rtc13ThreadManager8InstanceEv_nid_postfix() {
    static WebRtcThreadManager* const manager = CreateThreadManager();
    return manager;
}

void* APS5_VABI _ZN3rtc13ThreadManager13CurrentThreadEv_nid_postfix(WebRtcThreadManager* manager) {
    if (!manager)
        throw std::invalid_argument("ThreadManager::CurrentThread requires a manager");
    return pthread_getspecific_nid_postfix(manager->key);
}

void* APS5_VABI _ZN3rtc6Thread7CurrentEv_nid_postfix() {
    return _ZN3rtc13ThreadManager13CurrentThreadEv_nid_postfix(_ZN3rtc13ThreadManager8InstanceEv_nid_postfix());
}

bool APS5_VABI _ZNK3rtc6Thread9IsCurrentEv_nid_postfix(const void* thread) {
    return _ZN3rtc13ThreadManager13CurrentThreadEv_nid_postfix(_ZN3rtc13ThreadManager8InstanceEv_nid_postfix()) == thread;
}

}
