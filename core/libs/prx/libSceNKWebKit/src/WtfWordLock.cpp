// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WTF/wtf/WordLock.cpp
/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {

constexpr std::uintptr_t wordLocked = 1;
constexpr std::uintptr_t wordQueueLocked = 2;
constexpr std::uintptr_t wordFlags = 3;

struct WordWaiter {
    std::mutex mutex;
    std::condition_variable condition;
    bool waiting{true};
    WordWaiter* next{nullptr};
    WordWaiter* tail{nullptr};
};

static_assert(sizeof(std::uintptr_t) == 8 && alignof(WordWaiter) >= 4);
static_assert(std::atomic_ref<std::uintptr_t>::required_alignment <= alignof(std::uintptr_t));

}  // namespace

extern "C" {

void APS5_VABI _ZN3WTF8WordLock8lockSlowEv(std::uintptr_t* self) {
    if (!self) throw std::runtime_error("WTF::WordLock: null lock");
    std::atomic_ref word(*self);
    unsigned spins = 0;
    for (;;) {
        auto value = word.load();
        if (!(value & wordLocked)) {
            if (value & wordQueueLocked) throw std::runtime_error("WTF::WordLock: queue locked without ownership");
            if (word.compare_exchange_weak(value, value | wordLocked)) return;
            continue;
        }
        if (!(value & ~wordFlags) && spins < 40) {
            ++spins;
            std::this_thread::yield();
            continue;
        }
        WordWaiter waiter;
        value = word.load();
        if ((value & wordQueueLocked) || !(value & wordLocked) || !word.compare_exchange_weak(value, value | wordQueueLocked)) {
            std::this_thread::yield();
            continue;
        }
        auto* head = reinterpret_cast<WordWaiter*>(value & ~wordFlags);
        if (head) {
            head->tail->next = &waiter;
            head->tail = &waiter;
            word.store(value);
        } else {
            waiter.tail = &waiter;
            word.store(reinterpret_cast<std::uintptr_t>(&waiter) | wordLocked);
        }
        std::unique_lock lock(waiter.mutex);
        waiter.condition.wait(lock, [&] { return !waiter.waiting; });
    }
}

void APS5_VABI _ZN3WTF8WordLock10unlockSlowEv(std::uintptr_t* self) {
    if (!self) throw std::runtime_error("WTF::WordLock: null lock");
    std::atomic_ref word(*self);
    for (;;) {
        auto value = word.load();
        if (!(value & wordLocked)) throw std::runtime_error("WTF::WordLock: unlock of an unlocked lock");
        if (value == wordLocked) {
            if (word.compare_exchange_weak(value, 0)) return;
        } else if (!(value & wordQueueLocked) && word.compare_exchange_weak(value, value | wordQueueLocked)) {
            auto* head = reinterpret_cast<WordWaiter*>(value & ~wordFlags);
            auto* next = head->next;
            if (next) next->tail = head->tail;
            word.store(reinterpret_cast<std::uintptr_t>(next));
            head->next = nullptr;
            head->tail = nullptr;
            std::lock_guard lock(head->mutex);
            head->waiting = false;
            head->condition.notify_one();
            return;
        }
        std::this_thread::yield();
    }
}

}
