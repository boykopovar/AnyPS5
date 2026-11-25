//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// https://github.com/llvm/llvm-project/blob/6009708b4367171ccdbf4b5905cb6a803753fe18/libcxx/src/shared_mutex.cpp

#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdint>
#include <stdexcept>

namespace {

struct GuestSharedMutex {
    PthreadMutex mutex;
    PthreadCond gate1;
    PthreadCond gate2;
    unsigned state;
    unsigned padding;
};
static_assert(sizeof(GuestSharedMutex) == 32);

struct GuestUniqueLock {
    PthreadMutex* mutex;
    bool owns;
    std::uint8_t padding[7]{};
};
static_assert(sizeof(GuestUniqueLock) == 16);

constexpr unsigned writeEntered = 1U << 31;
constexpr unsigned readerMask = ~writeEntered;

GuestSharedMutex* Checked(GuestSharedMutex* self) {
    if (reinterpret_cast<std::uintptr_t>(self) % alignof(GuestSharedMutex) != 0 ||
        !GuestWritable_nid_no_patch(self, sizeof(*self)))
        throw std::invalid_argument("invalid shared mutex address");
    return self;
}

class Guard {
public:
    explicit Guard(GuestSharedMutex* self) : lock{&self->mutex, true} {
        _ZNSt3__15mutex4lockEv_nid_postfix(lock.mutex);
    }

    ~Guard() { _ZNSt3__15mutex6unlockEv_nid_postfix(lock.mutex); }

    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;

    void Wait(PthreadCond* condition) {
        _ZNSt3__118condition_variable4waitERNS_11unique_lockINS_5mutexEEE_nid_postfix(condition, &lock);
    }

private:
    GuestUniqueLock lock;
};

}

extern "C" {

void APS5_VABI _ZNSt3__119__shared_mutex_baseC1Ev_nid_postfix(void* selfAddress) {
    auto* self = Checked(static_cast<GuestSharedMutex*>(selfAddress));
    self->mutex = nullptr;
    self->gate1 = nullptr;
    self->gate2 = nullptr;
    self->state = 0;
}

void APS5_VABI _ZNSt3__119__shared_mutex_base4lockEv_nid_postfix(void* selfAddress) {
    auto* self = Checked(static_cast<GuestSharedMutex*>(selfAddress));
    Guard guard(self);
    while (self->state & writeEntered) guard.Wait(&self->gate1);
    self->state |= writeEntered;
    while (self->state & readerMask) guard.Wait(&self->gate2);
}

void APS5_VABI _ZNSt3__119__shared_mutex_base6unlockEv_nid_postfix(void* selfAddress) {
    auto* self = Checked(static_cast<GuestSharedMutex*>(selfAddress));
    Guard guard(self);
    self->state = 0;
    _ZNSt3__118condition_variable10notify_allEv_nid_postfix(&self->gate1);
}

void APS5_VABI _ZNSt3__119__shared_mutex_base11lock_sharedEv_nid_postfix(void* selfAddress) {
    auto* self = Checked(static_cast<GuestSharedMutex*>(selfAddress));
    Guard guard(self);
    while ((self->state & writeEntered) || (self->state & readerMask) == readerMask)
        guard.Wait(&self->gate1);
    self->state = (self->state & writeEntered) | ((self->state & readerMask) + 1);
}

void APS5_VABI _ZNSt3__119__shared_mutex_base13unlock_sharedEv_nid_postfix(void* selfAddress) {
    auto* self = Checked(static_cast<GuestSharedMutex*>(selfAddress));
    Guard guard(self);
    const unsigned readers = (self->state & readerMask) - 1;
    self->state = (self->state & writeEntered) | readers;
    if (self->state & writeEntered) {
        if (readers == 0) _ZNSt3__118condition_variable10notify_oneEv_nid_postfix(&self->gate2);
    } else if (readers == readerMask - 1) {
        _ZNSt3__118condition_variable10notify_oneEv_nid_postfix(&self->gate1);
    }
}

}
