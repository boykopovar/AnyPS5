/*	$KAME: rthdr.c,v 1.19 2003/06/06 10:48:51 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


/* Adapted from FreeBSD release/11.4.0 lib/libc/net/rthdr.c: https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/net/rthdr.c */
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

constexpr int IpProtoIpv6 = 41;
constexpr int Ipv6Rthdr = 51;
constexpr int Ipv6RthdrTypeZero = 0;
constexpr int Ipv6RthdrLoose = 0;
constexpr int Ipv6RthdrStrict = 1;
constexpr std::uint32_t CmsgHeaderBytes = 16;
constexpr std::uint32_t Rthdr0HeaderBytes = 8;
constexpr std::uint32_t InAddr6Bytes = 16;
constexpr int MaximumRthdr0Segments = 23;

std::uint32_t CmsgAlign(std::uint32_t value) {
    return (value + 7) & ~std::uint32_t{7};
}

std::uint32_t CmsgLen(std::uint32_t length) {
    return CmsgHeaderBytes + length;
}

std::uint32_t CmsgSpace(std::uint32_t length) {
    return CmsgHeaderBytes + CmsgAlign(length);
}

std::uint8_t* CmsgData(GuestCmsghdr* cmsg) {
    return reinterpret_cast<std::uint8_t*>(cmsg) + CmsgHeaderBytes;
}

const std::uint8_t* CmsgData(const GuestCmsghdr* cmsg) {
    return reinterpret_cast<const std::uint8_t*>(cmsg) + CmsgHeaderBytes;
}

GuestRthdr* Rthdr(void* bytes) {
    return static_cast<GuestRthdr*>(bytes);
}

const GuestRthdr* Rthdr(const void* bytes) {
    return static_cast<const GuestRthdr*>(bytes);
}

GuestRthdr0* Rthdr0(void* bytes) {
    return static_cast<GuestRthdr0*>(bytes);
}

const GuestRthdr0* Rthdr0(const void* bytes) {
    return static_cast<const GuestRthdr0*>(bytes);
}

GuestInAddr6* AddressAt(GuestRthdr0* header, std::size_t index) {
    return reinterpret_cast<GuestInAddr6*>(reinterpret_cast<std::uint8_t*>(header) + Rthdr0HeaderBytes) + index;
}

const GuestInAddr6* AddressAt(const GuestRthdr0* header, std::size_t index) {
    return reinterpret_cast<const GuestInAddr6*>(reinterpret_cast<const std::uint8_t*>(header) + Rthdr0HeaderBytes) + index;
}

}

extern "C" std::size_t APS5_VABI inet6_rthdr_space_nid_postfix(int type, int seg) {
    switch (type) {
    case Ipv6RthdrTypeZero:
        if (seg < 1 || seg > MaximumRthdr0Segments) return 0;
        return CmsgSpace(InAddr6Bytes * static_cast<std::uint32_t>(seg) + Rthdr0HeaderBytes);
    default:
        return 0;
    }
}

extern "C" GuestCmsghdr* APS5_VABI inet6_rthdr_init_nid_postfix(void* bp, int type) {
    auto* ch = static_cast<GuestCmsghdr*>(bp);
    auto* rthdr = reinterpret_cast<GuestRthdr*>(CmsgData(ch));
    ch->cmsg_level = IpProtoIpv6;
    ch->cmsg_type = Ipv6Rthdr;
    switch (type) {
    case Ipv6RthdrTypeZero:
        ch->cmsg_len = CmsgLen(Rthdr0HeaderBytes);
        std::memset(rthdr, 0, Rthdr0HeaderBytes);
        rthdr->ip6r_type = Ipv6RthdrTypeZero;
        return ch;
    default:
        return nullptr;
    }
}

extern "C" int APS5_VABI inet6_rthdr_add_nid_postfix(GuestCmsghdr* cmsg, const GuestInAddr6* addr, unsigned int flags) {
    auto* rthdr = reinterpret_cast<GuestRthdr*>(CmsgData(cmsg));
    switch (rthdr->ip6r_type) {
    case Ipv6RthdrTypeZero: {
        auto* rt0 = reinterpret_cast<GuestRthdr0*>(rthdr);
        if (flags != Ipv6RthdrLoose && flags != Ipv6RthdrStrict) return -1;
        if (rt0->ip6r0_segleft == MaximumRthdr0Segments) return -1;
        if (flags != Ipv6RthdrLoose) return -1;
        rt0->ip6r0_segleft++;
        std::memcpy(reinterpret_cast<std::uint8_t*>(rt0) + ((rt0->ip6r0_len + 1) << 3), addr, InAddr6Bytes);
        rt0->ip6r0_len += static_cast<std::uint8_t>(InAddr6Bytes >> 3);
        cmsg->cmsg_len = CmsgLen(static_cast<std::uint32_t>((rt0->ip6r0_len + 1) << 3));
        break;
    }
    default:
        return -1;
    }
    return 0;
}

extern "C" int APS5_VABI inet6_rthdr_lasthop_nid_postfix(GuestCmsghdr* cmsg, unsigned int flags) {
    auto* rthdr = reinterpret_cast<GuestRthdr*>(CmsgData(cmsg));
    switch (rthdr->ip6r_type) {
    case Ipv6RthdrTypeZero: {
        auto* rt0 = reinterpret_cast<GuestRthdr0*>(rthdr);
        if (rt0->ip6r0_segleft > MaximumRthdr0Segments) return -1;
        if (flags != Ipv6RthdrLoose) return -1;
        break;
    }
    default:
        return -1;
    }
    return 0;
}

extern "C" int APS5_VABI inet6_rthdr_segments_nid_postfix(const GuestCmsghdr* cmsg) {
    const auto* rthdr = reinterpret_cast<const GuestRthdr*>(CmsgData(cmsg));
    switch (rthdr->ip6r_type) {
    case Ipv6RthdrTypeZero: {
        const auto* rt0 = reinterpret_cast<const GuestRthdr0*>(rthdr);
        if (rt0->ip6r0_len % 2 || 46 < rt0->ip6r0_len) return -1;
        return static_cast<int>((rt0->ip6r0_len * 8) / InAddr6Bytes);
    }
    default:
        return -1;
    }
}

extern "C" GuestInAddr6* APS5_VABI inet6_rthdr_getaddr_nid_postfix(GuestCmsghdr* cmsg, int idx) {
    auto* rthdr = reinterpret_cast<GuestRthdr*>(CmsgData(cmsg));
    switch (rthdr->ip6r_type) {
    case Ipv6RthdrTypeZero: {
        auto* rt0 = reinterpret_cast<GuestRthdr0*>(rthdr);
        if (rt0->ip6r0_len % 2 || 46 < rt0->ip6r0_len) return nullptr;
        const int naddr = static_cast<int>((rt0->ip6r0_len * 8) / InAddr6Bytes);
        if (idx <= 0 || naddr < idx) return nullptr;
        return AddressAt(rt0, static_cast<std::size_t>(idx));
    }
    default:
        return nullptr;
    }
}

extern "C" int APS5_VABI inet6_rthdr_getflags_nid_postfix(const GuestCmsghdr* cmsg, int idx) {
    const auto* rthdr = reinterpret_cast<const GuestRthdr*>(CmsgData(cmsg));
    switch (rthdr->ip6r_type) {
    case Ipv6RthdrTypeZero: {
        const auto* rt0 = reinterpret_cast<const GuestRthdr0*>(rthdr);
        if (rt0->ip6r0_len % 2 || 46 < rt0->ip6r0_len) return -1;
        const int naddr = static_cast<int>((rt0->ip6r0_len * 8) / InAddr6Bytes);
        if (idx < 0 || naddr < idx) return -1;
        return Ipv6RthdrLoose;
    }
    default:
        return -1;
    }
}

extern "C" std::uint32_t APS5_VABI inet6_rth_space_nid_postfix(int type, int segments) {
    switch (type) {
    case Ipv6RthdrTypeZero:
        if (segments >= 0 && segments <= 127) return static_cast<std::uint32_t>(((segments * 2) + 1) << 3);
        return 0;
    default:
        return 0;
    }
}

extern "C" void* APS5_VABI inet6_rth_init_nid_postfix(void* bp, std::uint32_t bp_len, int type, int segments) {
    switch (type) {
    case Ipv6RthdrTypeZero: {
        if (bp_len < inet6_rth_space_nid_postfix(Ipv6RthdrTypeZero, segments)) return nullptr;
        if (segments < 0 || segments > 127) return nullptr;
        std::memset(bp, 0, bp_len);
        auto* rth0 = Rthdr0(bp);
        rth0->ip6r0_len = static_cast<std::uint8_t>(segments * 2);
        rth0->ip6r0_type = Ipv6RthdrTypeZero;
        rth0->ip6r0_segleft = 0;
        rth0->ip6r0_reserved = 0;
        break;
    }
    default:
        return nullptr;
    }
    return bp;
}

extern "C" int APS5_VABI inet6_rth_add_nid_postfix(void* bp, const GuestInAddr6* addr) {
    auto* rth = Rthdr(bp);
    switch (rth->ip6r_type) {
    case Ipv6RthdrTypeZero: {
        auto* rth0 = Rthdr0(bp);
        if (rth0->ip6r0_segleft == (rth0->ip6r0_len / 2)) return -1;
        GuestInAddr6* nextaddr = AddressAt(rth0, rth0->ip6r0_segleft);
        *nextaddr = *addr;
        rth0->ip6r0_segleft++;
        break;
    }
    default:
        return -1;
    }
    return 0;
}

extern "C" int APS5_VABI inet6_rth_reverse_nid_postfix(const void* in, void* out) {
    const auto* rth_in = Rthdr(in);
    switch (rth_in->ip6r_type) {
    case Ipv6RthdrTypeZero: {
        const auto* rth0_in = Rthdr0(in);
        auto* rth0_out = Rthdr0(out);
        if (rth0_in->ip6r0_len % 2) return -1;
        const int segments = rth0_in->ip6r0_len / 2;
        std::memmove(rth0_out, rth0_in, ((rth0_in->ip6r0_len) + 1) << 3);
        rth0_out->ip6r0_segleft = static_cast<std::uint8_t>(segments);
        for (int i = 0; i < segments / 2; i++) {
            GuestInAddr6* addr1 = AddressAt(rth0_out, static_cast<std::size_t>(i));
            GuestInAddr6* addr2 = AddressAt(rth0_out, static_cast<std::size_t>(segments - i - 1));
            GuestInAddr6 addrTmp = *addr1;
            *addr1 = *addr2;
            *addr2 = addrTmp;
        }
        break;
    }
    default:
        return -1;
    }
    return 0;
}

extern "C" int APS5_VABI inet6_rth_segments_nid_postfix(const void* bp) {
    const auto* rh = Rthdr(bp);
    switch (rh->ip6r_type) {
    case Ipv6RthdrTypeZero: {
        const auto* rh0 = Rthdr0(bp);
        int addrs;
        if ((rh0->ip6r0_len % 2) != 0 || (addrs = (rh0->ip6r0_len >> 1)) < rh0->ip6r0_segleft) return -1;
        return addrs;
    }
    default:
        return -1;
    }
}

extern "C" GuestInAddr6* APS5_VABI inet6_rth_getaddr_nid_postfix(const void* bp, int idx) {
    const auto* rh = Rthdr(bp);
    switch (rh->ip6r_type) {
    case Ipv6RthdrTypeZero: {
        const auto* rh0 = Rthdr0(bp);
        int addrs;
        if ((rh0->ip6r0_len % 2) != 0 || (addrs = (rh0->ip6r0_len >> 1)) < rh0->ip6r0_segleft) return nullptr;
        if (idx < 0 || addrs <= idx) return nullptr;
        return const_cast<GuestInAddr6*>(AddressAt(rh0, static_cast<std::size_t>(idx)));
    }
    default:
        return nullptr;
    }
}
