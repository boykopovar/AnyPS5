/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2014 University of Washington. All rights reserved.
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
 * Derived from Source/WTF/wtf/JSONValues.cpp and JSONValues.h@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/include/WtfJsonExports.hpp"

#include <stdexcept>
#include <string>

namespace {

using Jsc::WtfAbi::JsonArrayBase;
using Jsc::WtfAbi::JsonType;
using Jsc::WtfAbi::JsonValue;
using Jsc::WtfAbi::String;
using Jsc::WtfAbi::StringImpl;

template<typename T> T& Require(T* value, const char* function) {
    if (!value) throw std::invalid_argument(std::string(function) + ": null WTF JSON argument");
    return *value;
}

void DerefValue(JsonValue* value) {
    if (!value) return;
    if (value->refCount != 1) {
        --value->refCount;
        return;
    }
    if (!value->vtable || !value->vtable[Jsc::WtfAbi::JsonDeletingDestructorSlot])
        throw std::invalid_argument("WTF::JSONImpl::Value::deref: value without a deleting destructor");
    reinterpret_cast<Jsc::WtfAbi::JsonDeletingDestructor>(value->vtable[Jsc::WtfAbi::JsonDeletingDestructorSlot])(value);
}

void DestroyValueMembers(JsonValue& value) {
    if (value.type == JsonType::String) Jsc::WtfAbi::DerefString(value.value.string);
}

}  // namespace

extern "C" {

bool APS5_VABI _ZN3WTF8JSONImpl5Value7asValueERNS_6RefPtrIS1_NS_13DumbPtrTraitsIS1_EEEE(JsonValue* self, JsonValue** output) {
    JsonValue& value = Require(self, __func__);
    JsonValue*& target = Require(output, __func__);
    ++value.refCount;
    JsonValue* previous = target;
    target = &value;
    DerefValue(previous);
    return true;
}

bool APS5_VABI _ZNK3WTF8JSONImpl5Value9asBooleanERb(const JsonValue* self, bool* output) {
    const JsonValue& value = Require(self, __func__);
    bool& target = Require(output, __func__);
    if (value.type != JsonType::Boolean) return false;
    target = value.value.boolean;
    return true;
}

bool APS5_VABI _ZNK3WTF8JSONImpl5Value8asStringERNS_6StringE(const JsonValue* self, String* output) {
    const JsonValue& value = Require(self, __func__);
    String& target = Require(output, __func__);
    if (value.type != JsonType::String) return false;
    StringImpl* string = value.value.string ? Jsc::WtfAbi::RefString(*value.value.string) : nullptr;
    StringImpl* previous = target.impl;
    target.impl = string;
    Jsc::WtfAbi::DerefString(previous);
    return true;
}

unsigned APS5_VABI _ZNK3WTF8JSONImpl9ArrayBase6lengthEv(const JsonArrayBase* self) {
    return Require(self, __func__).size;
}

JsonValue* const* APS5_VABI _ZNK3WTF8JSONImpl9ArrayBase3endEv(const JsonArrayBase* self) {
    const JsonArrayBase& array = Require(self, __func__);
    return array.buffer + array.size;
}

void APS5_VABI _ZN3WTF8JSONImpl9ArrayBaseD2Ev(JsonArrayBase* self) {
    JsonArrayBase& array = Require(self, __func__);
    for (unsigned i = 0; i < array.size; ++i) DerefValue(array.buffer[i]);
    Jsc::WtfAbi::FreeBlock(array.buffer);
    array.buffer = nullptr;
    array.capacity = 0;
    DestroyValueMembers(array.base);
}
}
