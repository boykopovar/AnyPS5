/*
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "prx/libSceNKWebKit/include/WtfExports.hpp"

using namespace NKWebKit::WtfAbi;

extern "C" {

const URL& APS5_VABI _ZN3WTF13aboutBlankURLEv() {
    static StringImpl impl = [] {
        static const unsigned char characters[] = "about:blank";
        StringImpl value{1, 11, characters, 12};
        _ZNK3WTF10StringImpl12hashSlowCaseEv(&value);
        return value;
    }();
    static const URL value{&impl, (5U << 6) | 5U, 6, 6, 6, 6, 6, 11, 11};
    return value;
}

const URL& APS5_VABI _ZN3WTF14aboutSrcDocURLEv() {
    static StringImpl impl = [] {
        static const unsigned char characters[] = "about:srcdoc";
        StringImpl value{1, 12, characters, 12};
        _ZNK3WTF10StringImpl12hashSlowCaseEv(&value);
        return value;
    }();
    static const URL value{&impl, (5U << 6) | 5U, 6, 6, 6, 6, 6, 12, 12};
    return value;
}

}
