#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace Jsc::JscAbi {

using PackedPtrStorage = std::array<std::uint8_t, 6>;

struct SharedTaskVtable {
    void (APS5_VABI* completeDestructor)(void* self);
    void (APS5_VABI* deletingDestructor)(void* self);
    void (APS5_VABI* run)(void* self, void* data);
};

struct SharedTask {
    const SharedTaskVtable* vptr;
    std::atomic<unsigned> refCount;
};

struct SharedArrayBufferContents {
    std::atomic<unsigned> refCount;
    void* data;
    PackedPtrStorage destructor;
    unsigned sizeInBytes;
};

struct ArrayBufferContents {
    void* data;
    PackedPtrStorage destructor;
    PackedPtrStorage shared;
    unsigned sizeInBytes;
};

static_assert(sizeof(SharedTaskVtable) == 24);
static_assert(offsetof(SharedTask, refCount) == 8);
static_assert(sizeof(std::atomic<unsigned>) == 4);
static_assert(sizeof(SharedArrayBufferContents) == 32);
static_assert(offsetof(SharedArrayBufferContents, data) == 8);
static_assert(offsetof(SharedArrayBufferContents, destructor) == 16);
static_assert(offsetof(SharedArrayBufferContents, sizeInBytes) == 24);
static_assert(sizeof(ArrayBufferContents) == 24);
static_assert(alignof(ArrayBufferContents) == 8);
static_assert(offsetof(ArrayBufferContents, data) == 0);
static_assert(offsetof(ArrayBufferContents, destructor) == 8);
static_assert(offsetof(ArrayBufferContents, shared) == 14);
static_assert(offsetof(ArrayBufferContents, sizeInBytes) == 20);

} // namespace Jsc::JscAbi

extern "C" {

void APS5_VABI _ZN3JSC19ArrayBufferContentsC1Ev(Jsc::JscAbi::ArrayBufferContents* self);
void APS5_VABI _ZN3JSC19ArrayBufferContentsC2Ev(Jsc::JscAbi::ArrayBufferContents* self);
void APS5_VABI _ZN3JSC19ArrayBufferContentsC1EOS0_(Jsc::JscAbi::ArrayBufferContents* self, Jsc::JscAbi::ArrayBufferContents* other);
void APS5_VABI _ZN3JSC19ArrayBufferContentsC2EOS0_(Jsc::JscAbi::ArrayBufferContents* self, Jsc::JscAbi::ArrayBufferContents* other);
Jsc::JscAbi::ArrayBufferContents* APS5_VABI _ZN3JSC19ArrayBufferContentsaSEOS0_(Jsc::JscAbi::ArrayBufferContents* self, Jsc::JscAbi::ArrayBufferContents* other);
void APS5_VABI _ZN3JSC19ArrayBufferContentsD1Ev(Jsc::JscAbi::ArrayBufferContents* self);
void APS5_VABI _ZN3JSC19ArrayBufferContentsD2Ev(Jsc::JscAbi::ArrayBufferContents* self);
void APS5_VABI _ZN3JSC19ArrayBufferContents5clearEv(Jsc::JscAbi::ArrayBufferContents* self);

}
