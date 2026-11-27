/*
 * Copyright (c) 1995 Bill Paul <wpaul@ctr.columbia.edu>.
 * Copyright (c) 2007 Robert N. M. Watson
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
 */

/* Adapted from FreeBSD release/11.4.0 lib/libc/net/ether_addr.c: https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/net/ether_addr.c */
#include "prx/libSceLibcInternal/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace {

constexpr std::size_t EtherTextBytes = 18;

}

extern "C" EtherAddress* APS5_VABI ether_aton_nid_postfix(const char* text) {
    static EtherAddress address;
    if (text == nullptr) return nullptr;
    unsigned int octets[6];
    if (std::sscanf(text, "%x:%x:%x:%x:%x:%x", &octets[0], &octets[1], &octets[2], &octets[3], &octets[4], &octets[5]) != 6) return nullptr;
    for (std::size_t i = 0; i < 6; ++i) address.octet[i] = static_cast<std::uint8_t>(octets[i]);
    return &address;
}

extern "C" char* APS5_VABI ether_ntoa_nid_postfix(const EtherAddress* address) {
    static char text[EtherTextBytes];
    if (address == nullptr) return nullptr;
    std::snprintf(text, sizeof(text), "%02x:%02x:%02x:%02x:%02x:%02x", static_cast<unsigned int>(address->octet[0]),
        static_cast<unsigned int>(address->octet[1]), static_cast<unsigned int>(address->octet[2]),
        static_cast<unsigned int>(address->octet[3]), static_cast<unsigned int>(address->octet[4]),
        static_cast<unsigned int>(address->octet[5]));
    return text;
}
