// Derived from Source/JavaScriptCore/runtime/ArrayBuffer.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2009-2020 Apple Inc. All rights reserved.
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
 *
 * Derived from Source/JavaScriptCore/runtime/ArrayBuffer.cpp@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libSceJsc/include/ArrayBufferContentsExports.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

using Jsc::JscAbi::ArrayBuffer;
using Jsc::JscAbi::ArrayBufferContents;
using Jsc::JscAbi::PackedPtrStorage;
using Jsc::JscAbi::SharedArrayBufferContents;
using Jsc::JscAbi::SharedTask;

constexpr unsigned MaxArrayBufferSize = 0x7fffffffu;

template<typename T> T* LoadPacked(const PackedPtrStorage& storage) {
    std::uintptr_t value = 0;
    std::memcpy(&value, storage.data(), storage.size());
    return reinterpret_cast<T*>(value);
}

void StorePacked(PackedPtrStorage& storage, const void* pointer) {
    const auto value = reinterpret_cast<std::uintptr_t>(pointer);
    if (value >> (storage.size() * 8)) throw std::invalid_argument("JSC::ArrayBufferContents: pointer does not fit in 48 bits");
    std::memcpy(storage.data(), &value, storage.size());
}

template<typename T> T* ExchangePacked(PackedPtrStorage& storage, T* pointer) {
    T* old = LoadPacked<T>(storage);
    StorePacked(storage, pointer);
    return old;
}

void Ref(SharedTask* task) {
    if (task) ++task->refCount;
}

void Deref(SharedTask* task) {
    if (!task || --task->refCount) return;
    task->refCount = 1;
    task->vptr->deletingDestructor(task);
}

void Ref(SharedArrayBufferContents* shared) {
    if (shared) ++shared->refCount;
}

void Deref(SharedArrayBufferContents* shared) {
    if (!shared || --shared->refCount) return;
    shared->refCount = 1;
    if (SharedTask* destructor = LoadPacked<SharedTask>(shared->destructor)) destructor->vptr->run(destructor, shared->data);
    Deref(ExchangePacked<SharedTask>(shared->destructor, nullptr));
    free_nid_postfix(shared);
}

ArrayBufferContents& Checked(ArrayBufferContents* self, const char* function) {
    if (!self) throw std::invalid_argument(std::string(function) + " requires an ArrayBufferContents");
    return *self;
}

void Destroy(ArrayBufferContents& self) {
    if (SharedTask* destructor = LoadPacked<SharedTask>(self.destructor)) destructor->vptr->run(destructor, self.data);
}

void Reset(ArrayBufferContents& self) {
    self.data = nullptr;
    Deref(ExchangePacked<SharedTask>(self.destructor, nullptr));
    Deref(ExchangePacked<SharedArrayBufferContents>(self.shared, nullptr));
    self.sizeInBytes = 0;
}

void Clear(ArrayBufferContents& self) {
    Destroy(self);
    Reset(self);
}

void TransferTo(ArrayBufferContents& self, ArrayBufferContents& other) {
    Clear(other);
    other.data = self.data;
    other.sizeInBytes = self.sizeInBytes;
    if (other.sizeInBytes > MaxArrayBufferSize) throw std::length_error("JSC::ArrayBufferContents: size exceeds MAX_ARRAY_BUFFER_SIZE");
    SharedTask* moved = ExchangePacked<SharedTask>(self.destructor, nullptr);
    Deref(ExchangePacked(other.destructor, moved));
    SharedArrayBufferContents* shared = LoadPacked<SharedArrayBufferContents>(self.shared);
    Ref(shared);
    Deref(ExchangePacked(other.shared, shared));
    Reset(self);
}

void Construct(ArrayBufferContents* self, const char* function) {
    ArrayBufferContents& contents = Checked(self, function);
    std::memset(&contents, 0, sizeof(contents));
    Reset(contents);
}

void MoveConstruct(ArrayBufferContents* self, ArrayBufferContents* other, const char* function) {
    Construct(self, function);
    TransferTo(Checked(other, function), *self);
}

void Destruct(ArrayBufferContents* self, const char* function) {
    ArrayBufferContents& contents = Checked(self, function);
    Destroy(contents);
    Deref(ExchangePacked<SharedArrayBufferContents>(contents.shared, nullptr));
    Deref(ExchangePacked<SharedTask>(contents.destructor, nullptr));
}

} // namespace

extern "C" {

void APS5_VABI _ZN3JSC19ArrayBufferContentsC1Ev(ArrayBufferContents* self) {
    Construct(self, __func__);
}

void APS5_VABI _ZN3JSC19ArrayBufferContentsC2Ev(ArrayBufferContents* self) {
    Construct(self, __func__);
}

void APS5_VABI _ZN3JSC19ArrayBufferContentsC1EOS0_(ArrayBufferContents* self, ArrayBufferContents* other) {
    MoveConstruct(self, other, __func__);
}

void APS5_VABI _ZN3JSC19ArrayBufferContentsC2EOS0_(ArrayBufferContents* self, ArrayBufferContents* other) {
    MoveConstruct(self, other, __func__);
}

ArrayBufferContents* APS5_VABI _ZN3JSC19ArrayBufferContentsaSEOS0_(ArrayBufferContents* self, ArrayBufferContents* other) {
    TransferTo(Checked(other, __func__), Checked(self, __func__));
    return self;
}

void APS5_VABI _ZN3JSC19ArrayBufferContentsD1Ev(ArrayBufferContents* self) {
    Destruct(self, __func__);
}

void APS5_VABI _ZN3JSC19ArrayBufferContentsD2Ev(ArrayBufferContents* self) {
    Destruct(self, __func__);
}

void APS5_VABI _ZN3JSC19ArrayBufferContents5clearEv(ArrayBufferContents* self) {
    Clear(Checked(self, __func__));
}

const char* APS5_VABI _ZN3JSC23errorMesasgeForTransferEPNS_11ArrayBufferE(ArrayBuffer* buffer) {
    if (!buffer) throw std::invalid_argument(std::string(__func__) + " requires an ArrayBuffer");
    if (LoadPacked<SharedArrayBufferContents>(buffer->contents.shared)) return "Cannot transfer a SharedArrayBuffer";
    if (buffer->isWasmMemory) return "Cannot transfer a WebAssembly.Memory";
    return "Cannot transfer an ArrayBuffer whose backing store has been accessed by the JavaScriptCore C API";
}

}
