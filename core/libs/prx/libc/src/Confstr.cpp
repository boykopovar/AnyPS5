/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* Adapted from FreeBSD release/11.4.0 lib/libc/gen/confstr.c: https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/confstr.c */
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstring>

namespace {

constexpr int GuestEinval = 22;
constexpr int CsPath = 1;
constexpr int CsPosixV6WidthRestrictedEnvs = 14;
constexpr int CsPosixV6IlpOff32First = 2;
constexpr int CsPosixV6IlpOff32Last = 4;
constexpr int CsPosixV6IlpOffBigFirst = 5;
constexpr int CsPosixV6IlpOffBigLast = 7;
constexpr int CsPosixV6Lp64First = 8;
constexpr int CsPosixV6Lp64Last = 10;
constexpr int CsPosixV6LpBigFirst = 11;
constexpr int CsPosixV6LpBigLast = 13;

constexpr std::size_t GuestLongBits = 64;
constexpr std::size_t GuestPointerBits = 64;
constexpr std::size_t GuestIntBits = 32;
constexpr std::size_t GuestOffTBytes = 8;
constexpr std::size_t GuestLongBytes = 8;

constexpr const char DefaultPath[] = "/usr/bin:/bin:/usr/sbin:/sbin";
constexpr const char UnsupportedEnvironment[] = "unsupported programming environment";
constexpr const char EmptyString[] = "";
constexpr const char Width64[] = "_POSIX_V6_LP64_OFF64";
constexpr const char Width32[] = "_POSIX_V6_ILP32_OFFBIG";

constexpr bool IlpOffBigSupported = GuestLongBits == 32 && GuestOffTBytes > GuestLongBytes;
constexpr bool Lp64OffSixtyFourSupported = GuestLongBits >= 64 && GuestPointerBits >= 64 && GuestIntBits >= 32 && GuestOffTBytes >= GuestLongBytes;

}

extern "C" std::size_t APS5_VABI confstr_nid_postfix(int name, char* buffer, std::size_t length) {
    const char* text = nullptr;
    if (name == CsPath) {
        text = DefaultPath;
    } else if (name >= CsPosixV6IlpOff32First && name <= CsPosixV6IlpOff32Last) {
        text = UnsupportedEnvironment;
    } else if (name >= CsPosixV6IlpOffBigFirst && name <= CsPosixV6IlpOffBigLast) {
        text = IlpOffBigSupported ? EmptyString : UnsupportedEnvironment;
    } else if (name >= CsPosixV6Lp64First && name <= CsPosixV6Lp64Last) {
        text = Lp64OffSixtyFourSupported ? EmptyString : UnsupportedEnvironment;
    } else if (name >= CsPosixV6LpBigFirst && name <= CsPosixV6LpBigLast) {
        text = UnsupportedEnvironment;
    } else if (name == CsPosixV6WidthRestrictedEnvs) {
        text = GuestLongBits >= 64 ? Width64 : Width32;
    } else {
        *__error_nid_postfix() = GuestEinval;
        return 0;
    }
    if (length != 0 && buffer != nullptr) strlcpy_nid_postfix(buffer, text, length);
    return std::strlen(text) + 1;
}
