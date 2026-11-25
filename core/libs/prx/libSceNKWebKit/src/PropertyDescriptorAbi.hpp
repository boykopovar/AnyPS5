// Derived from Source/JavaScriptCore/runtime/PropertyDescriptor.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from Source/JavaScriptCore/runtime/PropertySlot.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 *  Copyright (C) 2005-2022 Apple Inc. All rights reserved.
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

#pragma once

#include <cstddef>
#include <cstdint>

namespace NKWebKit::JscAbi {

enum PropertyAttribute : std::uint32_t {
    ReadOnly = 1u << 1,
    DontEnum = 1u << 2,
    DontDelete = 1u << 3,
    Accessor = 1u << 4
};

enum PropertyDescriptorPresent : std::uint32_t {
    WritablePresent = 1,
    EnumerablePresent = 2,
    ConfigurablePresent = 4
};

constexpr std::uint64_t EncodedJSUndefined = 0x2 | 0x8;

struct PropertyDescriptor {
    std::uint64_t value;
    std::uint64_t getter;
    std::uint64_t setter;
    std::uint32_t attributes;
    std::uint32_t seenAttributes;
};

static_assert(sizeof(PropertyDescriptor) == 0x20);
static_assert(offsetof(PropertyDescriptor, value) == 0x0);
static_assert(offsetof(PropertyDescriptor, getter) == 0x8);
static_assert(offsetof(PropertyDescriptor, setter) == 0x10);
static_assert(offsetof(PropertyDescriptor, attributes) == 0x18);
static_assert(offsetof(PropertyDescriptor, seenAttributes) == 0x1c);

}
