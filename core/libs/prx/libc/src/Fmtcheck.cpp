/*	$NetBSD: fmtcheck.c,v 1.8 2008/04/28 20:22:59 martin Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code was contributed to The NetBSD Foundation by Allen Briggs.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/* Adapted from FreeBSD release/11.4.0 lib/libc/gen/fmtcheck.c: https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/fmtcheck.c */
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstring>

namespace {

enum class Eft {
    Start,
    Short,
    Int,
    WInt,
    Long,
    Quad,
    IntMax,
    PtrDiff,
    Size,
    CharPointer,
    ShortPointer,
    IntPointer,
    LongPointer,
    QuadPointer,
    IntMaxPointer,
    PtrDiffPointer,
    SizePointer,
    Double,
    LongDouble,
    String,
    WString,
    Width,
    Precision,
    Done,
    Unknown,
};

enum class Modifier {
    None,
    Char,
    Short,
    Long,
    Quad,
    IntMax,
    LongDouble,
    PtrDiff,
    Size,
};

Eft Return(const char** position, const char* current, Eft result) {
    *position = current;
    return result;
}

bool IsDigit(char value) {
    return value >= '0' && value <= '9';
}

Eft NextFromPrecision(const char** position);

Eft NextFromWidth(const char** position) {
    const char* f = *position;
    if (*f == '.') {
        ++f;
        if (*f == '*') return Return(position, f, Eft::Precision);
        while (IsDigit(*f)) ++f;
        if (!*f) return Return(position, f, Eft::Unknown);
    }
    *position = f;
    return NextFromPrecision(position);
}

Eft NextFromPrecision(const char** position) {
    Modifier modifier;
    const char* f = *position;
    switch (*f) {
    case 'h':
        ++f;
        if (!*f) return Return(position, f, Eft::Unknown);
        if (*f == 'h') {
            ++f;
            modifier = Modifier::Char;
        } else {
            modifier = Modifier::Short;
        }
        break;
    case 'j':
        ++f;
        modifier = Modifier::IntMax;
        break;
    case 'l':
        ++f;
        if (!*f) return Return(position, f, Eft::Unknown);
        if (*f == 'l') {
            ++f;
            modifier = Modifier::Quad;
        } else {
            modifier = Modifier::Long;
        }
        break;
    case 'q':
        ++f;
        modifier = Modifier::Quad;
        break;
    case 't':
        ++f;
        modifier = Modifier::PtrDiff;
        break;
    case 'z':
        ++f;
        modifier = Modifier::Size;
        break;
    case 'L':
        ++f;
        modifier = Modifier::LongDouble;
        break;
    default:
        modifier = Modifier::None;
        break;
    }
    if (!*f) return Return(position, f, Eft::Unknown);
    if (std::strchr("diouxX", *f)) {
        switch (modifier) {
        case Modifier::Long:
            return Return(position, f, Eft::Long);
        case Modifier::Quad:
            return Return(position, f, Eft::Quad);
        case Modifier::IntMax:
            return Return(position, f, Eft::IntMax);
        case Modifier::PtrDiff:
            return Return(position, f, Eft::PtrDiff);
        case Modifier::Size:
            return Return(position, f, Eft::Size);
        case Modifier::Char:
        case Modifier::Short:
        case Modifier::None:
            return Return(position, f, Eft::Int);
        default:
            return Return(position, f, Eft::Unknown);
        }
    }
    if (*f == 'n') {
        switch (modifier) {
        case Modifier::Char:
            return Return(position, f, Eft::CharPointer);
        case Modifier::Short:
            return Return(position, f, Eft::ShortPointer);
        case Modifier::Long:
            return Return(position, f, Eft::LongPointer);
        case Modifier::Quad:
            return Return(position, f, Eft::QuadPointer);
        case Modifier::IntMax:
            return Return(position, f, Eft::IntMaxPointer);
        case Modifier::PtrDiff:
            return Return(position, f, Eft::PtrDiffPointer);
        case Modifier::Size:
            return Return(position, f, Eft::SizePointer);
        case Modifier::None:
            return Return(position, f, Eft::IntPointer);
        default:
            return Return(position, f, Eft::Unknown);
        }
    }
    if (std::strchr("DOU", *f)) {
        if (modifier != Modifier::None) return Return(position, f, Eft::Unknown);
        return Return(position, f, Eft::Long);
    }
    if (std::strchr("aAeEfFgG", *f)) {
        switch (modifier) {
        case Modifier::LongDouble:
            return Return(position, f, Eft::LongDouble);
        case Modifier::Long:
        case Modifier::None:
            return Return(position, f, Eft::Double);
        default:
            return Return(position, f, Eft::Unknown);
        }
    }
    if (*f == 'c') {
        switch (modifier) {
        case Modifier::Long:
            return Return(position, f, Eft::WInt);
        case Modifier::None:
            return Return(position, f, Eft::Int);
        default:
            return Return(position, f, Eft::Unknown);
        }
    }
    if (*f == 'C') {
        if (modifier != Modifier::None) return Return(position, f, Eft::Unknown);
        return Return(position, f, Eft::WInt);
    }
    if (*f == 's') {
        switch (modifier) {
        case Modifier::Long:
            return Return(position, f, Eft::WString);
        case Modifier::None:
            return Return(position, f, Eft::String);
        default:
            return Return(position, f, Eft::Unknown);
        }
    }
    if (*f == 'S') {
        if (modifier != Modifier::None) return Return(position, f, Eft::Unknown);
        return Return(position, f, Eft::WString);
    }
    if (*f == 'p') {
        if (modifier != Modifier::None) return Return(position, f, Eft::Unknown);
        return Return(position, f, Eft::Long);
    }
    return Return(position, f, Eft::Unknown);
}

Eft NextFormat(const char** position, Eft previous) {
    if (previous == Eft::Width) {
        ++*position;
        return NextFromWidth(position);
    }
    if (previous == Eft::Precision) {
        ++*position;
        return NextFromPrecision(position);
    }
    const char* f = *position;
    bool inFormat = false;
    while (!inFormat) {
        f = std::strchr(f, '%');
        if (f == nullptr) return Return(position, f, Eft::Done);
        ++f;
        if (!*f) return Return(position, f, Eft::Unknown);
        if (*f != '%') {
            inFormat = true;
        } else {
            ++f;
        }
    }
    while (*f && std::strchr("#'0- +", *f)) ++f;
    if (*f == '*') return Return(position, f, Eft::Width);
    while (IsDigit(*f)) ++f;
    if (!*f) return Return(position, f, Eft::Unknown);
    *position = f;
    return NextFromWidth(position);
}

}

extern "C" const char* APS5_VABI fmtcheck_nid_postfix(const char* f1, const char* f2) {
    if (!f1) return f2;
    const char* f1p = f1;
    Eft f1t = Eft::Start;
    const char* f2p = f2;
    Eft f2t = Eft::Start;
    while ((f1t = NextFormat(&f1p, f1t)) != Eft::Done) {
        if (f1t == Eft::Unknown) return f2;
        if (f2p == nullptr) return f2;
        f2t = NextFormat(&f2p, f2t);
        if (f1t != f2t) return f2;
    }
    return f1;
}
