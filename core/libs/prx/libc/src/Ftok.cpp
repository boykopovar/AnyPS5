/*
 * Copyright (c) 1994 SigmaSoft, Th. Lockert <tholo@sigmasoft.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/* Adapted from FreeBSD release/11.4.0 lib/libc/gen/ftok.c: https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/ftok.c */
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FilesystemForwards.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

namespace {

constexpr std::uint32_t KernelErrnoBase = 0x80020000u;
constexpr int NoSystemCall = 78;

}

extern "C" std::int64_t APS5_VABI ftok_nid_postfix(const char* path, int id) {
    const auto* forwards = RegisteredFilesystemForwards_nid_no_patch();
    if (!forwards) {
        *__error_nid_postfix() = NoSystemCall;
        return -1;
    }
    FileStat status{};
    if (const int result = forwards->stat(path, &status); result != 0) {
        const auto code = static_cast<std::uint32_t>(result);
        *__error_nid_postfix() = (code & 0xffff0000u) == KernelErrnoBase ? static_cast<int>(code & 0xffffu) : 1;
        return -1;
    }
    return static_cast<std::int64_t>(static_cast<std::uint32_t>(id) << 24 | (status.st_dev & 0xffu) << 16 | (status.st_ino & 0xffffu));
}
