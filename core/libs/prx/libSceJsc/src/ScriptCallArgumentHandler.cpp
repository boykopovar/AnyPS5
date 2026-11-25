/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Derived from Source/JavaScriptCore/bindings/ScriptFunctionCall.cpp@c55364d53f971f63a709976f168d932d558b8909
 */

/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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
 * Derived from Source/JavaScriptCore/runtime/ArgList.h@c55364d53f971f63a709976f168d932d558b8909
 */

/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2020 Apple Inc. All rights reserved.
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
 * Derived from Source/JavaScriptCore/runtime/JSCJSValue.h@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libSceJsc/include/ScriptCallArgumentHandlerExports.hpp"
#include "prx/libc/include/General.hpp"

#include <bit>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using Jsc::ScriptCallAbi::ScriptCallArgumentHandler;

constexpr std::uint64_t NumberTag = 0xfffe000000000000ull;
constexpr std::uint64_t DoubleEncodeOffset = 1ull << 49;
constexpr std::uint64_t ValueFalse = 0x06;
constexpr std::uint64_t ValueTrue = 0x07;
constexpr std::int32_t InlineCapacity = 8;

template<typename T> std::uint64_t EncodeNumber(T value) {
    if (std::in_range<std::int32_t>(value)) return NumberTag | static_cast<std::uint32_t>(static_cast<std::int32_t>(value));
    return std::bit_cast<std::uint64_t>(static_cast<double>(value)) + DoubleEncodeOffset;
}

ScriptCallArgumentHandler& Checked(ScriptCallArgumentHandler* self, const char* function) {
    if (!self) throw std::invalid_argument(std::string(function) + ": null this");
    return *self;
}

void Construct(ScriptCallArgumentHandler* self, void* globalObject, const char* function) {
    auto& arguments = Checked(self, function).arguments;
    arguments.overflowed = 0;
    arguments.size = 0;
    arguments.capacity = InlineCapacity;
    arguments.buffer = arguments.inlineBuffer;
    arguments.markSet = nullptr;
    self->globalObject = globalObject;
}

void Destroy(ScriptCallArgumentHandler* self, const char* function) {
    auto& arguments = Checked(self, function).arguments;
    if (arguments.markSet || arguments.buffer != arguments.inlineBuffer) NotImplemented_nid_no_patch(function);
}

void Append(ScriptCallArgumentHandler* self, std::uint64_t value, const char* function) {
    auto& arguments = Checked(self, function).arguments;
    if (arguments.size < 0 || arguments.size > arguments.capacity) throw std::invalid_argument(std::string(function) + ": corrupt argument buffer");
    if (arguments.size == arguments.capacity || arguments.buffer != arguments.inlineBuffer) NotImplemented_nid_no_patch(function);
    arguments.buffer[arguments.size++] = value;
}

} // namespace

extern "C" {

void APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandlerC1EPN3JSC14JSGlobalObjectE(ScriptCallArgumentHandler* self, void* globalObject) {
    Construct(self, globalObject, __func__);
}

void APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandlerC2EPN3JSC14JSGlobalObjectE(ScriptCallArgumentHandler* self, void* globalObject) {
    Construct(self, globalObject, __func__);
}

void APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandlerD1Ev(ScriptCallArgumentHandler* self) {
    Destroy(self, __func__);
}

void APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandlerD2Ev(ScriptCallArgumentHandler* self) {
    Destroy(self, __func__);
}

void APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEN3JSC7JSValueE(ScriptCallArgumentHandler* self, std::uint64_t argument) {
    Append(self, argument, __func__);
}

void APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEb(ScriptCallArgumentHandler* self, bool argument) {
    Append(self, argument ? ValueTrue : ValueFalse, __func__);
}

void APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEi(ScriptCallArgumentHandler* self, int argument) {
    Append(self, EncodeNumber(argument), __func__);
}

void APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEj(ScriptCallArgumentHandler* self, unsigned int argument) {
    Append(self, EncodeNumber(argument), __func__);
}

void APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEl(ScriptCallArgumentHandler* self, std::int64_t argument) {
    Append(self, EncodeNumber(argument), __func__);
}

void APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEm(ScriptCallArgumentHandler* self, std::uint64_t argument) {
    Append(self, EncodeNumber(argument), __func__);
}

void APS5_VABI _ZN10Deprecated25ScriptCallArgumentHandler14appendArgumentEx(ScriptCallArgumentHandler* self, std::int64_t argument) {
    Append(self, EncodeNumber(argument), __func__);
}

}
