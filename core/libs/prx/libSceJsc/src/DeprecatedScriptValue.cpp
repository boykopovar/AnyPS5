// Derived from Source/JavaScriptCore/heap/Strong.h@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2011-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Derived from Source/JavaScriptCore/heap/Strong.h@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libSceJsc/include/InventoryExports.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr std::size_t kHandleSlotSize = 8;

const void* HandleSlot(const void* value, const char* function) {
    if (!value) throw std::invalid_argument(std::string(function) + " requires a ScriptValue");
    const void* slot = nullptr;
    std::memcpy(&slot, value, kHandleSlotSize);
    return slot;
}

void CopyEmpty(void* self, const void* other, const char* function) {
    if (!self) throw std::invalid_argument(std::string(function) + " requires a ScriptValue");
    if (HandleSlot(other, function)) NotImplemented_nid_no_patch(function);
    std::memset(self, 0, kHandleSlotSize);
}

void DestroyEmpty(void* self, const char* function) {
    if (HandleSlot(self, function)) NotImplemented_nid_no_patch(function);
}

} // namespace

extern "C" {

void APS5_VABI _ZN10Deprecated11ScriptValueC1ERKS0_(void* self, const void* other) {
    CopyEmpty(self, other, __func__);
}

void APS5_VABI _ZN10Deprecated11ScriptValueC2ERKS0_(void* self, const void* other) {
    CopyEmpty(self, other, __func__);
}

void APS5_VABI _ZN10Deprecated11ScriptValueD1Ev(void* self) {
    DestroyEmpty(self, __func__);
}

void APS5_VABI _ZN10Deprecated11ScriptValueD2Ev(void* self) {
    DestroyEmpty(self, __func__);
}

void* APS5_VABI _ZN10Deprecated11ScriptValueaSERKS0_(void* self, const void* other) {
    if (HandleSlot(other, __func__) || HandleSlot(self, __func__)) NotImplemented_nid_no_patch(__func__);
    return self;
}

}
