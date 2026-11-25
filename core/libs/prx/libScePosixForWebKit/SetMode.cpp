/* Derived from FreeBSD lib/libc/gen/setmode.c (8c639945a445) */
/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Dave Borman at Cray Research, Inc.
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

#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace {

using ModeT = std::uint16_t;

struct BitCommand {
    char cmd;
    char cmd2;
    ModeT bits;
};

constexpr ModeT SIsuid = 04000;
constexpr ModeT SIsgid = 02000;
constexpr ModeT SIstxt = 01000;
constexpr ModeT SIrwxu = 0700;
constexpr ModeT SIrusr = 0400;
constexpr ModeT SIwusr = 0200;
constexpr ModeT SIxusr = 0100;
constexpr ModeT SIrwxg = 070;
constexpr ModeT SIrgrp = 040;
constexpr ModeT SIwgrp = 020;
constexpr ModeT SIxgrp = 010;
constexpr ModeT SIrwxo = 07;
constexpr ModeT SIroth = 04;
constexpr ModeT SIwoth = 02;
constexpr ModeT SIxoth = 01;
constexpr ModeT SIfdir = 040000;
constexpr ModeT StandardBits = SIsuid | SIsgid | SIrwxu | SIrwxg | SIrwxo;

constexpr int Cmd2Clear = 0x01;
constexpr int Cmd2Set = 0x02;
constexpr int Cmd2Gbits = 0x04;
constexpr int Cmd2Obits = 0x08;
constexpr int Cmd2Ubits = 0x10;

constexpr std::size_t SetLength = 6;
constexpr std::size_t SetLengthIncrement = 4;
constexpr int GuestEnomem = 12;
constexpr int GuestEinval = 22;
constexpr int GuestErange = 34;

void SetError(int error) {
    *__error_nid_postfix() = error;
}

ModeT GetUmask() {
    const ModeT previous = umask_nid_postfix(0);
    umask_nid_postfix(previous);
    return previous;
}

BitCommand* AddCommand(BitCommand* set, int op, ModeT who, ModeT oparg, ModeT mask) {
    switch (op) {
        case '=':
            set->cmd = '-';
            set->cmd2 = 0;
            set->bits = who ? who : StandardBits;
            ++set;
            op = '+';
            [[fallthrough]];
        case '+':
        case '-':
        case 'X':
            set->cmd = static_cast<char>(op);
            set->cmd2 = 0;
            set->bits = static_cast<ModeT>((who ? who : mask) & oparg);
            break;
        case 'u':
        case 'g':
        case 'o':
            set->cmd = static_cast<char>(op);
            if (who) {
                set->cmd2 = static_cast<char>(((who & SIrusr) ? Cmd2Ubits : 0) | ((who & SIrgrp) ? Cmd2Gbits : 0) |
                                              ((who & SIroth) ? Cmd2Obits : 0));
                set->bits = static_cast<ModeT>(~0);
            } else {
                set->cmd2 = static_cast<char>(Cmd2Ubits | Cmd2Gbits | Cmd2Obits);
                set->bits = mask;
            }
            if (oparg == '+') {
                set->cmd2 = static_cast<char>(set->cmd2 | Cmd2Set);
            } else if (oparg == '-') {
                set->cmd2 = static_cast<char>(set->cmd2 | Cmd2Clear);
            } else if (oparg == '=') {
                set->cmd2 = static_cast<char>(set->cmd2 | Cmd2Set | Cmd2Clear);
            }
            break;
        default:
            break;
    }
    return set + 1;
}

struct CommandList {
    BitCommand* base = nullptr;
    BitCommand* next = nullptr;
    BitCommand* end = nullptr;
    std::size_t length = 0;
};

bool Add(CommandList& list, int op, ModeT who, ModeT oparg, ModeT mask) {
    if (list.next >= list.end) {
        list.length += SetLengthIncrement;
        auto* grown = static_cast<BitCommand*>(reallocarray_nid_postfix(list.base, list.length, sizeof(BitCommand)));
        if (grown == nullptr) return false;
        list.next = grown + (list.next - list.base);
        list.base = grown;
        list.end = grown + (list.length - 2);
    }
    list.next = AddCommand(list.next, op, who, oparg, mask);
    return true;
}

void* Fail(CommandList& list, int error) {
    if (list.base != nullptr) free_nid_postfix(list.base);
    SetError(error);
    return nullptr;
}

void CompressMode(BitCommand* set) {
    BitCommand* next = set;
    for (;;) {
        int op;
        while ((op = next->cmd) != '+' && op != '-' && op != 'X') {
            *set++ = *next++;
            if (!op) return;
        }
        int setbits = 0;
        int clrbits = 0;
        int Xbits = 0;
        for (;; ++next) {
            op = next->cmd;
            if (op == '-') {
                clrbits |= next->bits;
                setbits &= ~next->bits;
                Xbits &= ~next->bits;
            } else if (op == '+') {
                setbits |= next->bits;
                clrbits &= ~next->bits;
                Xbits &= ~next->bits;
            } else if (op == 'X') {
                Xbits |= next->bits & ~setbits;
            } else {
                break;
            }
        }
        if (clrbits) {
            set->cmd = '-';
            set->cmd2 = 0;
            set->bits = static_cast<ModeT>(clrbits);
            ++set;
        }
        if (setbits) {
            set->cmd = '+';
            set->cmd2 = 0;
            set->bits = static_cast<ModeT>(setbits);
            ++set;
        }
        if (Xbits) {
            set->cmd = 'X';
            set->cmd2 = 0;
            set->bits = static_cast<ModeT>(Xbits);
            ++set;
        }
    }
}

ModeT GetModeCommands(const BitCommand* set, ModeT omode) {
    ModeT newmode = omode;
    for (;; ++set) {
        ModeT value = 0;
        switch (set->cmd) {
            case 'u':
                value = static_cast<ModeT>((newmode & SIrwxu) >> 6);
                break;
            case 'g':
                value = static_cast<ModeT>((newmode & SIrwxg) >> 3);
                break;
            case 'o':
                value = static_cast<ModeT>(newmode & SIrwxo);
                break;
            case '+':
                newmode = static_cast<ModeT>(newmode | set->bits);
                continue;
            case '-':
                newmode = static_cast<ModeT>(newmode & ~set->bits);
                continue;
            case 'X':
                if (omode & (SIfdir | SIxusr | SIxgrp | SIxoth)) newmode = static_cast<ModeT>(newmode | set->bits);
                continue;
            case '\0':
            default:
                return newmode;
        }
        if (set->cmd2 & Cmd2Clear) {
            const ModeT clrval = (set->cmd2 & Cmd2Set) ? SIrwxo : value;
            if (set->cmd2 & Cmd2Ubits) newmode = static_cast<ModeT>(newmode & ~((clrval << 6) & set->bits));
            if (set->cmd2 & Cmd2Gbits) newmode = static_cast<ModeT>(newmode & ~((clrval << 3) & set->bits));
            if (set->cmd2 & Cmd2Obits) newmode = static_cast<ModeT>(newmode & ~(clrval & set->bits));
        }
        if (set->cmd2 & Cmd2Set) {
            if (set->cmd2 & Cmd2Ubits) newmode = static_cast<ModeT>(newmode | ((value << 6) & set->bits));
            if (set->cmd2 & Cmd2Gbits) newmode = static_cast<ModeT>(newmode | ((value << 3) & set->bits));
            if (set->cmd2 & Cmd2Obits) newmode = static_cast<ModeT>(newmode | (value & set->bits));
        }
    }
}

}

extern "C" void* APS5_VABI setmode_nid_postfix(const char* p) {
    if (p == nullptr) throw std::invalid_argument("setmode: null mode string");
    if (!*p) {
        SetError(GuestEinval);
        return nullptr;
    }
    const ModeT mask = static_cast<ModeT>(~GetUmask());
    CommandList list;
    list.length = SetLength + 2;
    list.base = static_cast<BitCommand*>(malloc_nid_postfix(list.length * sizeof(BitCommand)));
    if (list.base == nullptr) {
        SetError(GuestEnomem);
        return nullptr;
    }
    list.next = list.base;
    list.end = list.base + (list.length - 2);

    if (std::isdigit(static_cast<unsigned char>(*p))) {
        errno = 0;
        char* end = nullptr;
        const long perml = std::strtol(p, &end, 8);
        if (*end != '\0') return Fail(list, GuestEinval);
        if (errno == ERANGE && (perml == LONG_MAX || perml == LONG_MIN)) return Fail(list, GuestErange);
        if (perml & ~static_cast<long>(StandardBits | SIstxt)) return Fail(list, GuestEinval);
        const ModeT perm = static_cast<ModeT>(perml);
        if (!Add(list, '=', static_cast<ModeT>(StandardBits | SIstxt), perm, mask)) return Fail(list, GuestEnomem);
        list.next->cmd = 0;
        return list.base;
    }

    bool equalOpDone = false;
    bool parseWho = true;
    ModeT who = 0;
    for (;;) {
        if (parseWho) {
            who = 0;
            for (;; ++p) {
                if (*p == 'a') {
                    who = static_cast<ModeT>(who | StandardBits);
                } else if (*p == 'u') {
                    who = static_cast<ModeT>(who | SIsuid | SIrwxu);
                } else if (*p == 'g') {
                    who = static_cast<ModeT>(who | SIsgid | SIrwxg);
                } else if (*p == 'o') {
                    who = static_cast<ModeT>(who | SIrwxo);
                } else {
                    break;
                }
            }
        }
        parseWho = true;
        const int op = *p++;
        if (op != '+' && op != '-' && op != '=') return Fail(list, GuestEinval);
        if (op == '=') equalOpDone = false;
        who = static_cast<ModeT>(who & ~SIstxt);
        ModeT perm = 0;
        ModeT permXbits = 0;
        for (;; ++p) {
            bool wordEnd = false;
            switch (*p) {
                case 'r':
                    perm = static_cast<ModeT>(perm | SIrusr | SIrgrp | SIroth);
                    break;
                case 's':
                    if (!who || (who & ~SIrwxo)) perm = static_cast<ModeT>(perm | SIsuid | SIsgid);
                    break;
                case 't':
                    if (!who || (who & ~SIrwxo)) {
                        who = static_cast<ModeT>(who | SIstxt);
                        perm = static_cast<ModeT>(perm | SIstxt);
                    }
                    break;
                case 'w':
                    perm = static_cast<ModeT>(perm | SIwusr | SIwgrp | SIwoth);
                    break;
                case 'X':
                    permXbits = SIxusr | SIxgrp | SIxoth;
                    break;
                case 'x':
                    perm = static_cast<ModeT>(perm | SIxusr | SIxgrp | SIxoth);
                    break;
                case 'u':
                case 'g':
                case 'o':
                    if (perm) {
                        if (!Add(list, op, who, perm, mask)) return Fail(list, GuestEnomem);
                        perm = 0;
                    }
                    if (op == '=') equalOpDone = true;
                    if (op == '+' && permXbits) {
                        if (!Add(list, 'X', who, permXbits, mask)) return Fail(list, GuestEnomem);
                        permXbits = 0;
                    }
                    if (!Add(list, *p, who, static_cast<ModeT>(op), mask)) return Fail(list, GuestEnomem);
                    break;
                default:
                    if (perm || (op == '=' && !equalOpDone)) {
                        if (op == '=') equalOpDone = true;
                        if (!Add(list, op, who, perm, mask)) return Fail(list, GuestEnomem);
                        perm = 0;
                    }
                    if (permXbits) {
                        if (!Add(list, 'X', who, permXbits, mask)) return Fail(list, GuestEnomem);
                        permXbits = 0;
                    }
                    wordEnd = true;
                    break;
            }
            if (wordEnd) break;
        }
        if (!*p) break;
        if (*p != ',') {
            parseWho = false;
            continue;
        }
        ++p;
    }
    list.next->cmd = 0;
    CompressMode(list.base);
    return list.base;
}

extern "C" std::uint16_t APS5_VABI getmode_nid_postfix(const void* bbox, std::uint16_t omode) {
    if (bbox == nullptr) throw std::invalid_argument("getmode: null command list");
    return GetModeCommands(static_cast<const BitCommand*>(bbox), omode);
}
