/*
 * Copyright (C) 2007-2017 Apple Inc. All rights reserved.
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
 *
 * Derived from Source/WTF/wtf/posix/FileSystemPOSIX.cpp@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceJsc/include/WtfFileSystemExports.hpp"

#include <cstddef>
#include <cstdint>

namespace {

constexpr int GuestEintr = 4;

} // namespace

extern "C" {

int APS5_VABI _ZN3WTF14FileSystemImpl12readFromFileEiPci(int handle, char* data, int length) {
    do {
        const std::int64_t bytesRead = read_nid_postfix(handle, data, static_cast<std::size_t>(length));
        if (bytesRead >= 0) return static_cast<int>(bytesRead);
    } while (*__error_nid_postfix() == GuestEintr);
    return -1;
}

}
