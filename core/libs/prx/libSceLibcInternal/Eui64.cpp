/*
 * Copyright 2004 The Aerospace Corporation.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions, and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions, and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  The name of The Aerospace Corporation may not be used to endorse or
 *     promote products derived from this software.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AEROSPACE CORPORATION "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AEROSPACE CORPORATION BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (c) 1995
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
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
 *
 * EUI-64 conversion and lookup routines
 *
 *
 * Converted from ether_addr.c rev
 * FreeBSD: src/lib/libc/net/eui64.c,v 1.15 2002/04/08 07:51:10 ru Exp
 * by Brooks Davis
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Center for Telecommunications Research
 * Columbia University, New York City
 */

/* Adapted from FreeBSD release/11.4.0 lib/libc/net/eui64.c: https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/net/eui64.c */
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {

constexpr std::size_t Eui64TextBytes = 23;

}

extern "C" int APS5_VABI eui64_aton_nid_postfix(const char* text, GuestEui64* id) {
    if (text == nullptr) throw std::runtime_error("eui64_aton: null text");
    unsigned int o0 = 0, o1 = 0, o2 = 0, o3 = 0, o4 = 0, o5 = 0, o6 = 0, o7 = 0;
    int converted = std::sscanf(text, "%x-%x-%x-%x-%x-%x-%x-%x", &o0, &o1, &o2, &o3, &o4, &o5, &o6, &o7);
    if (converted != 8) converted = std::sscanf(text, "%x:%x:%x:%x:%x:%x:%x:%x", &o0, &o1, &o2, &o3, &o4, &o5, &o6, &o7);
    if (converted != 8) converted = std::sscanf(text, "0x%2x%2x%2x%2x%2x%2x%2x%2x", &o0, &o1, &o2, &o3, &o4, &o5, &o6, &o7);
    if (converted != 8) {
        converted = std::sscanf(text, "%x-%x-%x-%x-%x-%x", &o0, &o1, &o2, &o5, &o6, &o7);
        if (converted == 6) {
            o3 = 0xff;
            o4 = 0xfe;
        }
    }
    if (converted != 8 && converted != 6) {
        converted = std::sscanf(text, "%x:%x:%x:%x:%x:%x", &o0, &o1, &o2, &o5, &o6, &o7);
        if (converted == 6) {
            o3 = 0xff;
            o4 = 0xfe;
        }
    }
    if (converted != 8 && converted != 6) return -1;
    const unsigned int octets[8] = {o0, o1, o2, o3, o4, o5, o6, o7};
    for (std::size_t i = 0; i < 8; ++i) id->octet[i] = static_cast<std::uint8_t>(octets[i]);
    return 0;
}

extern "C" int APS5_VABI eui64_ntoa_nid_postfix(const GuestEui64* id, char* text, std::size_t length) {
    char formatted[Eui64TextBytes + 1];
    std::snprintf(formatted, sizeof(formatted), "%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x",
        static_cast<unsigned int>(id->octet[0]), static_cast<unsigned int>(id->octet[1]),
        static_cast<unsigned int>(id->octet[2]), static_cast<unsigned int>(id->octet[3]),
        static_cast<unsigned int>(id->octet[4]), static_cast<unsigned int>(id->octet[5]),
        static_cast<unsigned int>(id->octet[6]), static_cast<unsigned int>(id->octet[7]));
    if (length > 0) {
        const std::size_t written = length - 1 < Eui64TextBytes ? length - 1 : Eui64TextBytes;
        if (!GuestCopyOut_nid_no_patch(text, formatted, written)) return -1;
        const char terminator = '\0';
        if (!GuestCopyOut_nid_no_patch(text + written, &terminator, 1)) return -1;
    }
    return Eui64TextBytes < length ? 0 : -1;
}
