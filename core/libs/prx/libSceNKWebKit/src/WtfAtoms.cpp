// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WTF/wtf/text/AtomStringImpl.cpp
/*
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2015 Yusuke Suzuki<utatane.tea@gmail.com>. All rights reserved.
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

// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WTF/wtf/text/AtomStringTable.cpp
/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WTF/wtf/text/AtomString.cpp
/*
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "WtfAtoms.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>

namespace NKWebKit::Atoms {

AtomTable::~AtomTable() {
    for (const auto& entry : entries) entry.second->hashAndFlags &= ~16U;
}


void RemoveAtom_nid_no_patch(StringImpl* impl) {
    auto& atomTable = AtomTable_nid_no_patch();
    auto [first, last] = atomTable.entries.equal_range(impl->hashAndFlags >> 8);
    for (auto it = first; it != last; ++it) {
        if (it->second == impl) {
            atomTable.entries.erase(it);
            return;
        }
    }
    throw std::invalid_argument("WTF atom belongs to another thread");
}

StringImpl* FindAtom_nid_no_patch(StringView view, unsigned hash) {
    auto& atomTable = AtomTable_nid_no_patch();
    auto [first, last] = atomTable.entries.equal_range(hash);
    for (auto it = first; it != last; ++it)
        if (EqualViews_nid_no_patch(View_nid_no_patch(it->second), view)) return it->second;
    return nullptr;
}

template<typename Make> String Intern(StringView view, Make make) {
    unsigned hash = Hash_nid_no_patch(view);
    if (auto* found = FindAtom_nid_no_patch(view, hash)) return String(Retain_nid_no_patch(found));
    auto result = make();
    AtomTable_nid_no_patch().entries.emplace(hash, result.impl);
    result.impl->hashAndFlags = (result.impl->hashAndFlags & 255) | (hash << 8) | 16;
    return result;
}

String AddAtomBuffer_nid_no_patch(StringView view) {
    if (!view.data) return String();
    if (!view.length) return String(EmptyImpl_nid_no_patch());
    return Intern(view, [&] { return String(CopyImpl_nid_no_patch(view, true)); });
}

String AddStaticAtom_nid_no_patch(StringView view) {
    if (!view.data || !view.length) throw std::invalid_argument("empty WTF static atom");
    return Intern(view, [&] {
        return String(new (NKWebKit::Allocate(sizeof(StringImpl))) StringImpl{2, view.length, view.data, view.is8Bit ? 4U : 0U});
    });
}

void RequireAtomStorage_nid_no_patch(const StringImpl* impl, const char* function) {
    const auto* owner = impl;
    if ((impl->hashAndFlags & 3) == 2)
        owner = *reinterpret_cast<StringImpl* const*>(reinterpret_cast<const unsigned char*>(impl) + 24);
    if (((impl->hashAndFlags | owner->hashAndFlags) & 32) || (owner->hashAndFlags & 3) == 3)
        throw std::runtime_error(std::string(function) + ": symbol and external string storage is not modelled");
}

String AddAtom_nid_no_patch(StringImpl* impl, const char* function) {
    if (!impl) return String();
    if (!impl->length) return String(EmptyImpl_nid_no_patch());
    if (impl->refCount & 1) return AddStaticAtom_nid_no_patch(View_nid_no_patch(impl));
    RequireAtomStorage_nid_no_patch(impl, function);
    if (impl->hashAndFlags & 16) return String(Retain_nid_no_patch(impl));
    return Intern(View_nid_no_patch(impl), [&] { return String(Retain_nid_no_patch(impl)); });
}


}  // namespace NKWebKit::Atoms

using namespace NKWebKit::Atoms;

extern "C" {

String APS5_VABI _ZN3WTF14AtomStringImpl3addEPNS_10StringImplEjj(StringImpl* impl, unsigned start, unsigned length) {
    if (!impl) return String();
    if (!length || start >= impl->length) return String(EmptyImpl_nid_no_patch());
    length = std::min(length, impl->length - start);
    if (!start && length == impl->length) return AddAtom_nid_no_patch(impl, __func__);
    RequireAtomStorage_nid_no_patch(impl, __func__);
    auto view = View_nid_no_patch(impl);
    view.data = static_cast<const unsigned char*>(view.data) + std::size_t(start) * (view.is8Bit ? 1 : 2);
    view.length = length;
    return Intern(view, [&] {
        String owner(Retain_nid_no_patch(impl));
        return SharedSubstring_nid_no_patch(&owner, start, length);
    });
}

String APS5_VABI _ZN3WTF10AtomString16fromUTF8InternalEPKcS2_(const char* start, const char* end) {
    if (!start) throw std::invalid_argument("null WTF atom UTF-8 input");
    if (!end) end = start + std::strlen(start);
    if (reinterpret_cast<std::uintptr_t>(end) <= reinterpret_cast<std::uintptr_t>(start))
        throw std::invalid_argument("empty WTF atom UTF-8 range");
    auto string = FromUTF8_nid_no_patch(reinterpret_cast<const unsigned char*>(start), static_cast<std::size_t>(end - start));
    return AddAtom_nid_no_patch(string.impl, __func__);
}

}
