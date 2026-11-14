/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 *
 * Derived from Source/WTF/wtf/PageBlock.cpp and Source/WTF/wtf/PageBlock.h@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceJsc/include/WtfPageBlockExports.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

constexpr int ScPageSize = 47;
constexpr std::size_t CeilingOnPageSize = 16 * 1024;

bool IsPowerOfTwo(std::size_t value) {
    return value && !(value & (value - 1));
}

std::size_t SystemPageSize() {
    return static_cast<std::size_t>(sysconf_nid_postfix(ScPageSize));
}

std::size_t PageSize() {
    static const std::size_t pageSize = [] {
        const std::size_t size = SystemPageSize();
        if (!IsPowerOfTwo(size)) throw std::runtime_error("WTF::pageSize: the system page size is not a power of two");
        if (size > CeilingOnPageSize) throw std::runtime_error("WTF::pageSize: the system page size is above CeilingOnPageSize");
        return size;
    }();
    return pageSize;
}

} // namespace

extern "C" {

std::size_t APS5_VABI _ZN3WTF8pageMaskEv() {
    static const std::size_t pageMask = ~(PageSize() - 1);
    return pageMask;
}

}
