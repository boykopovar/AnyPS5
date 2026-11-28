/*	$KAME: ip6opt.c,v 1.13 2003/06/06 10:08:20 suz Exp $	*/

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


/* Adapted from FreeBSD release/11.4.0 lib/libc/net/ip6opt.c: https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/net/ip6opt.c */
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

constexpr int IpProtoIpv6 = 41;
constexpr int Ipv6HopOpts = 49;
constexpr int Ipv6DstOpts = 50;
constexpr int Ipv62292HopOpts = 22;
constexpr int Ipv62292DstOpts = 23;
constexpr std::uint8_t Ip6OptPad1 = 0x00;
constexpr std::uint8_t Ip6OptPadN = 0x01;
constexpr std::uint32_t CmsgHeaderBytes = 16;

std::uint32_t CmsgAlign(std::uint32_t value) {
    return (value + 7) & ~std::uint32_t{7};
}

std::uint32_t CmsgLen(std::uint32_t length) {
    return CmsgHeaderBytes + length;
}

std::uint32_t CmsgSpace(std::uint32_t length) {
    return CmsgHeaderBytes + CmsgAlign(length);
}

bool IsHopOptions(int type) {
    return type == Ipv6HopOpts || type == Ipv62292HopOpts;
}

bool IsDstOptions(int type) {
    return type == Ipv6DstOpts || type == Ipv62292DstOpts;
}

std::uint8_t* CmsgData(GuestCmsghdr* cmsg) {
    return reinterpret_cast<std::uint8_t*>(cmsg) + CmsgHeaderBytes;
}

const std::uint8_t* CmsgData(const GuestCmsghdr* cmsg) {
    return reinterpret_cast<const std::uint8_t*>(cmsg) + CmsgHeaderBytes;
}

int RoundUp2(int value, int multiple) {
    return (value + (multiple - 1)) & ~(multiple - 1);
}

int OptionLength(const std::uint8_t* option, const std::uint8_t* limit) {
    int optlen;
    if (*option == Ip6OptPad1) {
        optlen = 1;
    } else {
        if (option + 2 > limit) return 0;
        optlen = *(option + 1) + 2;
    }
    if (option + optlen <= limit) return optlen;
    return 0;
}

void InsertPadOption(std::uint8_t* p, int length) {
    switch (length) {
    case 0:
        return;
    case 1:
        p[0] = Ip6OptPad1;
        return;
    default:
        p[0] = Ip6OptPadN;
        p[1] = static_cast<std::uint8_t>(length - 2);
        std::memset(&p[2], 0, static_cast<std::size_t>(length - 2));
        return;
    }
}

}

extern "C" int APS5_VABI inet6_option_space_nid_postfix(int nbytes) {
    nbytes += 2;
    return static_cast<int>(CmsgSpace(static_cast<std::uint32_t>((nbytes + 7) & ~7)));
}

extern "C" int APS5_VABI inet6_option_init_nid_postfix(void* bp, GuestCmsghdr** cmsgp, int type) {
    auto* ch = static_cast<GuestCmsghdr*>(bp);
    if (!IsHopOptions(type) && !IsDstOptions(type)) return -1;
    ch->cmsg_level = IpProtoIpv6;
    ch->cmsg_type = type;
    ch->cmsg_len = CmsgLen(0);
    *cmsgp = ch;
    return 0;
}

extern "C" int APS5_VABI inet6_option_append_nid_postfix(GuestCmsghdr* cmsg, const std::uint8_t* typep, int multx, int plusy) {
    int padlen, optlen, off;
    std::uint8_t* bp = reinterpret_cast<std::uint8_t*>(cmsg) + cmsg->cmsg_len;
    auto* eh = reinterpret_cast<GuestIp6Ext*>(CmsgData(cmsg));
    if (multx != 1 && multx != 2 && multx != 4 && multx != 8) return -1;
    if (plusy < 0 || plusy > 7) return -1;
    if (bp == reinterpret_cast<std::uint8_t*>(eh)) {
        bp += 2;
        cmsg->cmsg_len += 2;
    }
    off = static_cast<int>(bp - reinterpret_cast<std::uint8_t*>(eh));
    padlen = RoundUp2(off % multx, multx) - (off % multx);
    padlen += plusy;
    padlen %= multx;
    InsertPadOption(bp, padlen);
    cmsg->cmsg_len += static_cast<std::uint32_t>(padlen);
    bp += padlen;
    if (typep[0] == Ip6OptPad1) {
        optlen = 1;
    } else {
        optlen = typep[1] + 2;
    }
    std::memcpy(bp, typep, static_cast<std::size_t>(optlen));
    bp += optlen;
    cmsg->cmsg_len += static_cast<std::uint32_t>(optlen);
    off = static_cast<int>(bp - reinterpret_cast<std::uint8_t*>(eh));
    padlen = ((off + 7) & ~7) - off;
    InsertPadOption(bp, padlen);
    bp += padlen;
    cmsg->cmsg_len += static_cast<std::uint32_t>(padlen);
    eh->ip6e_len = static_cast<std::uint8_t>(((bp - reinterpret_cast<std::uint8_t*>(eh)) >> 3) - 1);
    return 0;
}

extern "C" std::uint8_t* APS5_VABI inet6_option_alloc_nid_postfix(GuestCmsghdr* cmsg, int datalen, int multx, int plusy) {
    int padlen, off;
    std::uint8_t* bp = reinterpret_cast<std::uint8_t*>(cmsg) + cmsg->cmsg_len;
    std::uint8_t* retval;
    auto* eh = reinterpret_cast<GuestIp6Ext*>(CmsgData(cmsg));
    if (multx != 1 && multx != 2 && multx != 4 && multx != 8) return nullptr;
    if (plusy < 0 || plusy > 7) return nullptr;
    if (bp == reinterpret_cast<std::uint8_t*>(eh)) {
        bp += 2;
        cmsg->cmsg_len += 2;
    }
    off = static_cast<int>(bp - reinterpret_cast<std::uint8_t*>(eh));
    padlen = RoundUp2(off % multx, multx) - (off % multx);
    padlen += plusy;
    padlen %= multx;
    InsertPadOption(bp, padlen);
    cmsg->cmsg_len += static_cast<std::uint32_t>(padlen);
    bp += padlen;
    retval = bp;
    bp += datalen;
    cmsg->cmsg_len += static_cast<std::uint32_t>(datalen);
    off = static_cast<int>(bp - reinterpret_cast<std::uint8_t*>(eh));
    padlen = ((off + 7) & ~7) - off;
    InsertPadOption(bp, padlen);
    bp += padlen;
    cmsg->cmsg_len += static_cast<std::uint32_t>(padlen);
    eh->ip6e_len = static_cast<std::uint8_t>(((bp - reinterpret_cast<std::uint8_t*>(eh)) >> 3) - 1);
    return retval;
}

extern "C" int APS5_VABI inet6_option_next_nid_postfix(const GuestCmsghdr* cmsg, std::uint8_t** tptrp) {
    if (cmsg->cmsg_level != IpProtoIpv6 || (!IsHopOptions(cmsg->cmsg_type) && !IsDstOptions(cmsg->cmsg_type))) return -1;
    if (cmsg->cmsg_len < CmsgSpace(sizeof(GuestIp6Ext))) return -1;
    const auto* ip6e = reinterpret_cast<const GuestIp6Ext*>(CmsgData(cmsg));
    const int hdrlen = (ip6e->ip6e_len + 1) << 3;
    if (cmsg->cmsg_len < CmsgSpace(static_cast<std::uint32_t>(hdrlen))) return -1;
    const std::uint8_t* lim = reinterpret_cast<const std::uint8_t*>(ip6e) + hdrlen;
    if (*tptrp == nullptr) {
        *tptrp = const_cast<std::uint8_t*>(reinterpret_cast<const std::uint8_t*>(ip6e + 1));
    } else {
        int optlen;
        if ((optlen = OptionLength(*tptrp, lim)) == 0) return -1;
        *tptrp = *tptrp + optlen;
    }
    if (*tptrp >= lim) {
        *tptrp = nullptr;
        return -1;
    }
    if (OptionLength(*tptrp, lim) == 0) return -1;
    return 0;
}

extern "C" int APS5_VABI inet6_option_find_nid_postfix(const GuestCmsghdr* cmsg, std::uint8_t** tptrp, int type) {
    if (cmsg->cmsg_level != IpProtoIpv6 || (!IsHopOptions(cmsg->cmsg_type) && !IsDstOptions(cmsg->cmsg_type))) return -1;
    if (cmsg->cmsg_len < CmsgSpace(sizeof(GuestIp6Ext))) return -1;
    const auto* ip6e = reinterpret_cast<const GuestIp6Ext*>(CmsgData(cmsg));
    const int hdrlen = (ip6e->ip6e_len + 1) << 3;
    if (cmsg->cmsg_len < CmsgSpace(static_cast<std::uint32_t>(hdrlen))) return -1;
    const std::uint8_t* lim = reinterpret_cast<const std::uint8_t*>(ip6e) + hdrlen;
    int optlen;
    if (*tptrp == nullptr) {
        *tptrp = const_cast<std::uint8_t*>(reinterpret_cast<const std::uint8_t*>(ip6e + 1));
    } else {
        if ((optlen = OptionLength(*tptrp, lim)) == 0) return -1;
        *tptrp = *tptrp + optlen;
    }
    std::uint8_t* optp;
    for (optp = *tptrp; optp < lim; optp += optlen) {
        if (*optp == type) {
            *tptrp = optp;
            return 0;
        }
        if ((optlen = OptionLength(optp, lim)) == 0) return -1;
    }
    *tptrp = nullptr;
    return -1;
}

extern "C" int APS5_VABI inet6_opt_init_nid_postfix(void* extbuf, std::uint32_t extlen) {
    auto* ext = static_cast<GuestIp6Ext*>(extbuf);
    if (ext) {
        if (extlen == 0 || (extlen % 8)) return -1;
        ext->ip6e_len = static_cast<std::uint8_t>((extlen >> 3) - 1);
    }
    return 2;
}

extern "C" int APS5_VABI inet6_opt_append_nid_postfix(void* extbuf, std::uint32_t extlen, int offset, std::uint8_t type, std::uint32_t len, std::uint8_t align, void** databufp) {
    int currentlen = offset;
    int padlen = 0;
    if (type < 2) return -1;
    if (len > 255) return -1;
    if (align != 1 && align != 2 && align != 4 && align != 8) return -1;
    if (align > len) return -1;
    currentlen += 2 + static_cast<int>(len);
    if (currentlen % align) padlen = align - (currentlen % align);
    currentlen += padlen;
    if (extlen && static_cast<std::uint32_t>(currentlen) > extlen) return -1;
    if (extbuf) {
        std::uint8_t* optp = static_cast<std::uint8_t*>(extbuf) + offset;
        if (padlen == 1) {
            *optp = Ip6OptPad1;
            optp++;
        } else if (padlen > 0) {
            *optp++ = Ip6OptPadN;
            *optp++ = static_cast<std::uint8_t>(padlen - 2);
            std::memset(optp, 0, static_cast<std::size_t>(padlen - 2));
            optp += (padlen - 2);
        }
        *optp++ = type;
        *optp++ = static_cast<std::uint8_t>(len);
        *databufp = optp;
    }
    return currentlen;
}

extern "C" int APS5_VABI inet6_opt_finish_nid_postfix(void* extbuf, std::uint32_t extlen, int offset) {
    const int updatelen = offset > 0 ? (1 + ((offset - 1) | 7)) : 0;
    if (extbuf) {
        const int padlen = updatelen - offset;
        if (static_cast<std::uint32_t>(updatelen) > extlen) return -1;
        std::uint8_t* padp = static_cast<std::uint8_t*>(extbuf) + offset;
        if (padlen == 1) {
            *padp = Ip6OptPad1;
        } else if (padlen > 0) {
            *padp++ = Ip6OptPadN;
            *padp++ = static_cast<std::uint8_t>(padlen - 2);
            std::memset(padp, 0, static_cast<std::size_t>(padlen - 2));
        }
    }
    return updatelen;
}

extern "C" int APS5_VABI inet6_opt_set_val_nid_postfix(void* databuf, int offset, void* val, std::uint32_t vallen) {
    std::memcpy(static_cast<std::uint8_t*>(databuf) + offset, val, vallen);
    return offset + static_cast<int>(vallen);
}

extern "C" int APS5_VABI inet6_opt_next_nid_postfix(void* extbuf, std::uint32_t extlen, int offset, std::uint8_t* typep, std::uint32_t* lenp, void** databufp) {
    std::uint8_t* optp;
    const std::uint8_t* lim;
    int optlen;
    if (extlen == 0 || (extlen % 8)) return -1;
    lim = static_cast<std::uint8_t*>(extbuf) + extlen;
    if (offset == 0) {
        optp = static_cast<std::uint8_t*>(extbuf) + 2;
    } else {
        optp = static_cast<std::uint8_t*>(extbuf) + offset;
    }
    while (optp < lim) {
        switch (*optp) {
        case Ip6OptPad1:
            optp++;
            break;
        case Ip6OptPadN:
            if ((optlen = OptionLength(optp, lim)) == 0) goto optend;
            optp += optlen;
            break;
        default:
            if ((optlen = OptionLength(optp, lim)) == 0) goto optend;
            *typep = *optp;
            *lenp = static_cast<std::uint32_t>(optlen - 2);
            *databufp = optp + 2;
            return static_cast<int>(optp + optlen - static_cast<std::uint8_t*>(extbuf));
        }
    }
optend:
    *databufp = nullptr;
    return -1;
}

extern "C" int APS5_VABI inet6_opt_find_nid_postfix(void* extbuf, std::uint32_t extlen, int offset, std::uint8_t type, std::uint32_t* lenp, void** databufp) {
    std::uint8_t* optp;
    const std::uint8_t* lim;
    int optlen;
    if (extlen == 0 || (extlen % 8)) return -1;
    lim = static_cast<std::uint8_t*>(extbuf) + extlen;
    if (offset == 0) {
        optp = static_cast<std::uint8_t*>(extbuf) + 2;
    } else {
        optp = static_cast<std::uint8_t*>(extbuf) + offset;
    }
    while (optp < lim) {
        if ((optlen = OptionLength(optp, lim)) == 0) goto optend;
        if (*optp == type) {
            *lenp = static_cast<std::uint32_t>(optlen - 2);
            *databufp = optp + 2;
            return static_cast<int>(optp + optlen - static_cast<std::uint8_t*>(extbuf));
        }
        optp += optlen;
    }
optend:
    *databufp = nullptr;
    return -1;
}

extern "C" int APS5_VABI inet6_opt_get_val_nid_postfix(void* databuf, int offset, void* val, std::uint32_t vallen) {
    std::memcpy(val, static_cast<std::uint8_t*>(databuf) + offset, vallen);
    return offset + static_cast<int>(vallen);
}
