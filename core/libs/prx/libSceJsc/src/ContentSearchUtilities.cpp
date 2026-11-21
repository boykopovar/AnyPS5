// Derived from Source/JavaScriptCore/inspector/ContentSearchUtilities.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
// Derived from Source/WTF/wtf/Vector.h@c55364d53f971f63a709976f168d932d558b8909
/*
 *  Copyright (C) 2005-2020 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "prx/libSceJsc/include/ContentSearchUtilitiesExports.hpp"
#include "prx/libSceJsc/src/WtfFastMalloc.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

namespace {

using Jsc::WtfAbi::SizeVector;
using Jsc::WtfAbi::String;
using Jsc::WtfAbi::StringImpl;
using Jsc::WtfAbi::StringImplFlag8Bit;

constexpr std::size_t NotFound = static_cast<std::size_t>(-1);
constexpr std::size_t VectorMinCapacity = 16;
constexpr std::size_t VectorMaxCapacity = std::numeric_limits<unsigned>::max() / sizeof(std::size_t);

struct Text {
    const void* characters;
    unsigned length;
    bool is8Bit;
};

std::size_t FindLineFeed(const Text& text, unsigned start) {
    for (unsigned index = start; index < text.length; ++index) {
        const char16_t unit = text.is8Bit ? static_cast<const unsigned char*>(text.characters)[index]
                                          : static_cast<const char16_t*>(text.characters)[index];
        if (unit == u'\n') return index;
    }
    return NotFound;
}

void Append(std::vector<std::size_t>& values, std::size_t& capacity, std::size_t value) {
    if (values.size() == capacity) {
        const std::size_t expanded = std::max(values.size() + 1, std::max(VectorMinCapacity, capacity + capacity / 4 + 1));
        if (expanded > VectorMaxCapacity)
            throw std::length_error("Inspector::ContentSearchUtilities::lineEndings: Vector capacity overflow");
        capacity = expanded;
    }
    values.push_back(value);
}

}

extern "C" {

SizeVector* APS5_VABI _ZN9Inspector22ContentSearchUtilities11lineEndingsERKN3WTF6StringE(SizeVector* result, const String* text) {
    if (!result) throw std::invalid_argument("Inspector::ContentSearchUtilities::lineEndings: null return storage");
    if (!text) throw std::invalid_argument("Inspector::ContentSearchUtilities::lineEndings requires a string");

    const StringImpl* impl = text->impl;
    const Text view{impl ? impl->data : nullptr, impl ? impl->length : 0, !impl || (impl->hashAndFlags & StringImplFlag8Bit)};
    if (view.length && !view.characters)
        throw std::invalid_argument("Inspector::ContentSearchUtilities::lineEndings: string has no characters");

    std::vector<std::size_t> endings;
    std::size_t capacity = 0;
    std::size_t start = 0;
    while (start < view.length) {
        std::size_t nextStart = FindLineFeed(view, static_cast<unsigned>(start));
        if (nextStart == NotFound || nextStart == view.length - 1) {
            Append(endings, capacity, view.length);
            break;
        }
        nextStart += 1;
        Append(endings, capacity, nextStart);
        start = nextStart;
    }
    Append(endings, capacity, view.length);

    const auto* allocator = Jsc::WtfAbi::RegisteredFastMalloc();
    if (!allocator || !allocator->allocate || !allocator->deallocate)
        throw std::runtime_error("Inspector::ContentSearchUtilities::lineEndings: Vector storage requires libSceNKWebKit fastMalloc");
    auto* buffer = static_cast<std::size_t*>(allocator->allocate(capacity * sizeof(std::size_t)));
    if (!buffer) throw std::bad_alloc();
    std::memcpy(buffer, endings.data(), endings.size() * sizeof(std::size_t));
    *result = SizeVector{buffer, static_cast<unsigned>(capacity), static_cast<unsigned>(endings.size())};
    return result;
}

}
