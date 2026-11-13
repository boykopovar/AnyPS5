// Derived from Source/WTF/wtf/text/SymbolImpl.cpp (PS5-WebKit-11.x 80f77e94c63e)
/*
 * Copyright (C) 2016 Yusuke Suzuki <utatane.tea@gmail.com>.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <atomic>

namespace {

constexpr unsigned StringImplFlagCount = 8;
constexpr unsigned SymbolHashFlag = 1u << 31;

std::atomic<unsigned> nextHashForSymbol{0};

}

extern "C" unsigned APS5_VABI _ZN3WTF10SymbolImpl17nextHashForSymbolEv() {
    unsigned current = nextHashForSymbol.load(std::memory_order_relaxed);
    unsigned next;
    do {
        next = (current + (1u << StringImplFlagCount)) | SymbolHashFlag;
    } while (!nextHashForSymbol.compare_exchange_weak(current, next, std::memory_order_relaxed));
    return next;
}
