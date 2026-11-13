/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <new>
#include <stdexcept>
#include <vector>

namespace {

using namespace NKWebKit::WtfAbi;

template<typename CharacterType>
String Simplify(const String& string, bool (APS5_VABI *predicate)(char16_t)) {
    const auto& impl = *string.impl;
    if (impl.length && (!impl.data || !predicate)) throw std::invalid_argument("invalid WTF whitespace input");
    std::vector<CharacterType> data(impl.length);
    auto* from = static_cast<const CharacterType*>(impl.data);
    auto* fromEnd = impl.length ? from + impl.length : from;
    unsigned outc = 0;
    bool changedToSpace = false;
    auto* to = data.data();
    while (true) {
        while (from != fromEnd && predicate(*from)) {
            if (*from != ' ')
                changedToSpace = true;
            ++from;
        }
        while (from != fromEnd && !predicate(*from))
            to[outc++] = *from++;
        if (from != fromEnd)
            to[outc++] = ' ';
        else
            break;
    }
    if (outc && to[outc - 1] == ' ')
        --outc;
    if (outc == impl.length && !changedToSpace)
        return string;
    void* output;
    String result(TryCreateUninitialized(outc, sizeof(CharacterType) == 1, output));
    if (!result.impl) throw std::bad_alloc();
    for (unsigned i = 0; i < outc; ++i)
        static_cast<CharacterType*>(output)[i] = to[i];
    return result;
}

}  // namespace

extern "C" {

NKWebKit::WtfAbi::String APS5_VABI _ZNK3WTF6String18simplifyWhiteSpaceEPFbDsE(const NKWebKit::WtfAbi::String* self, bool (APS5_VABI *predicate)(char16_t)) {
    if (!self) throw std::invalid_argument("null WTF String");
    if (!self->impl) return NKWebKit::WtfAbi::String();
    if (self->impl->hashAndFlags & 4) return Simplify<unsigned char>(*self, predicate);
    return Simplify<char16_t>(*self, predicate);
}

}
