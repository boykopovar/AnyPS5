// Derived from Source/WTF/wtf/ParkingLot.cpp@e12a212a531aeddc11259e8515671cf5188e038d
// Derived from Source/WTF/wtf/Lock.cpp@e12a212a531aeddc11259e8515671cf5188e038d
// Derived from Source/WTF/wtf/LockAlgorithmInlines.h@e12a212a531aeddc11259e8515671cf5188e038d
// Derived from Source/WTF/wtf/Condition.h@e12a212a531aeddc11259e8515671cf5188e038d
// Derived from Source/WTF/wtf/WordLock.cpp@e12a212a531aeddc11259e8515671cf5188e038d
// Derived from Source/WTF/wtf/ReadWriteLock.cpp@e12a212a531aeddc11259e8515671cf5188e038d
// Derived from Source/WTF/wtf/threads/BinarySemaphore.cpp@e12a212a531aeddc11259e8515671cf5188e038d
// Derived from Source/WTF/wtf/WeakRandom.h@e12a212a531aeddc11259e8515671cf5188e038d
// Derived from Source/WTF/wtf/HashFunctions.h@e12a212a531aeddc11259e8515671cf5188e038d
/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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

/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

/*
 * Copyright (C) 2010-2016 Apple Inc. All rights reserved.
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
 */

/*
 * Copyright (C) 2009, 2015 Apple Inc. All rights reserved.
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
 * Vigna, Sebastiano (2014). "Further scramblings of Marsaglia's xorshift
 * generators". arXiv:1404.0390 (http://arxiv.org/abs/1404.0390)
 *
 * See also https://en.wikipedia.org/wiki/Xorshift.
 */

/*
 * Copyright (C) 2005-2019 Apple Inc. All rights reserved.
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/WtfParkingLotExports.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstdint>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using NKWebKit::WtfAbi::BeforeSleepLambda;
using NKWebKit::WtfAbi::BinarySemaphore;
using NKWebKit::WtfAbi::Condition;
using NKWebKit::WtfAbi::Lock;
using NKWebKit::WtfAbi::ParkResult;
using NKWebKit::WtfAbi::ReadWriteLock;
using NKWebKit::WtfAbi::UnparkCallbackLambda;
using NKWebKit::WtfAbi::UnparkResult;
using NKWebKit::WtfAbi::ValidationLambda;
using NKWebKit::WtfAbi::WordLock;
using WTF::TimeWithDynamicClockType;

constexpr unsigned BucketCount = 1024;
constexpr unsigned SpinLimit = 40;
constexpr double MaximumWaitSeconds = 60.0 * 60.0 * 24.0;

template<typename T>
T& Checked(T* pointer, const char* caller) {
    if (!pointer) throw std::invalid_argument(std::string("WTF::") + caller + ": null pointer");
    return *pointer;
}

struct ThreadData {
    std::mutex parkingLock;
    std::condition_variable parkingCondition;
    const void* address{nullptr};
    ThreadData* nextInQueue{nullptr};
    std::intptr_t token{0};
};

ThreadData& MyThreadData() {
    thread_local ThreadData data;
    return data;
}

enum class DequeueResult {
    Ignore,
    RemoveAndContinue,
    RemoveAndStop,
};

class WeakRandom {
public:
    explicit WeakRandom(unsigned seed) {
        if (!seed) seed = 1;
        low = seed;
        high = seed;
        Advance();
    }

    double Get() {
        const std::uint64_t value = Advance() & ((1ULL << 53) - 1);
        return value * (1.0 / (1ULL << 53));
    }

private:
    std::uint64_t Advance() {
        std::uint64_t x = low;
        const std::uint64_t y = high;
        low = y;
        x ^= x << 23;
        x ^= x >> 17;
        x ^= y ^ (y >> 26);
        high = x;
        return high + low;
    }

    std::uint64_t low;
    std::uint64_t high;
};

double MonotonicNow() {
    return _ZN3WTF13MonotonicTime3nowEv().m_value;
}

struct Bucket {
    Bucket() : random(static_cast<unsigned>(reinterpret_cast<std::intptr_t>(this))) {}

    void Enqueue(ThreadData* data) {
        if (queueTail) {
            queueTail->nextInQueue = data;
            queueTail = data;
            return;
        }
        queueHead = data;
        queueTail = data;
    }

    template<typename Functor>
    void GenericDequeue(const Functor& functor) {
        if (!queueHead) return;
        bool shouldContinue = true;
        ThreadData** currentPtr = &queueHead;
        ThreadData* previous = nullptr;
        const double time = MonotonicNow();
        const bool timeToBeFair = time > nextFairTime;
        bool didDequeue = false;
        while (shouldContinue) {
            ThreadData* current = *currentPtr;
            if (!current) break;
            const DequeueResult result = functor(current, timeToBeFair);
            if (result == DequeueResult::Ignore) {
                previous = current;
                currentPtr = &current->nextInQueue;
                continue;
            }
            if (result == DequeueResult::RemoveAndStop) shouldContinue = false;
            if (current == queueTail) queueTail = previous;
            didDequeue = true;
            *currentPtr = current->nextInQueue;
            current->nextInQueue = nullptr;
        }
        if (timeToBeFair && didDequeue) nextFairTime = time + random.Get() / 1000.0;
    }

    ThreadData* queueHead{nullptr};
    ThreadData* queueTail{nullptr};
    std::mutex lock;
    double nextFairTime{0.0};
    WeakRandom random;
};

unsigned HashAddress(const void* address) {
    std::uint64_t key = reinterpret_cast<std::uintptr_t>(address);
    key += ~(key << 32);
    key ^= (key >> 22);
    key += ~(key << 13);
    key ^= (key >> 8);
    key += (key << 3);
    key ^= (key >> 15);
    key += ~(key << 27);
    key ^= (key >> 31);
    return static_cast<unsigned>(key);
}

Bucket& BucketFor(const void* address) {
    static std::array<Bucket, BucketCount> buckets;
    return buckets[HashAddress(address) % BucketCount];
}

template<typename Functor>
bool EnqueueThread(const void* address, const Functor& functor) {
    auto& bucket = BucketFor(address);
    std::lock_guard<std::mutex> locker(bucket.lock);
    ThreadData* threadData = functor();
    if (!threadData) return false;
    bucket.Enqueue(threadData);
    return true;
}

template<typename DequeueFunctor, typename FinishFunctor>
bool DequeueThreads(const void* address, const DequeueFunctor& dequeueFunctor, const FinishFunctor& finishFunctor) {
    auto& bucket = BucketFor(address);
    std::lock_guard<std::mutex> locker(bucket.lock);
    bucket.GenericDequeue(dequeueFunctor);
    const bool result = bucket.queueHead;
    finishFunctor(result);
    return result;
}

void Wake(ThreadData& threadData) {
    std::lock_guard<std::mutex> locker(threadData.parkingLock);
    threadData.address = nullptr;
    threadData.parkingCondition.notify_one();
}

bool IsInfinite(const TimeWithDynamicClockType& time) {
    return time.m_value == std::numeric_limits<double>::infinity();
}

double SecondsUntil(const TimeWithDynamicClockType& timeout) {
    return timeout.m_value - _ZNK3WTF24TimeWithDynamicClockType16nowWithSameClockEv(&timeout).m_value;
}

template<typename Validation, typename BeforeSleep>
ParkResult ParkConditionally(const void* address, const Validation& validation, const BeforeSleep& beforeSleep, const TimeWithDynamicClockType& timeout) {
    ThreadData& me = MyThreadData();
    me.token = 0;
    if (me.address) throw std::logic_error("WTF::ParkingLot: the thread is already parked");
    const bool enqueued = EnqueueThread(address, [&]() -> ThreadData* {
        if (!validation()) return nullptr;
        me.address = address;
        return &me;
    });
    if (!enqueued) return ParkResult();
    beforeSleep();
    bool didGetDequeued;
    {
        std::unique_lock<std::mutex> locker(me.parkingLock);
        while (me.address) {
            if (IsInfinite(timeout)) {
                me.parkingCondition.wait(locker);
                continue;
            }
            const double remaining = SecondsUntil(timeout);
            if (!(remaining > 0)) break;
            me.parkingCondition.wait_for(locker, std::chrono::duration<double>(remaining < MaximumWaitSeconds ? remaining : MaximumWaitSeconds));
        }
        didGetDequeued = !me.address;
    }
    if (didGetDequeued) return ParkResult{true, me.token};
    bool didDequeue = false;
    DequeueThreads(
        address,
        [&](ThreadData* element, bool) {
            if (element != &me) return DequeueResult::Ignore;
            didDequeue = true;
            return DequeueResult::RemoveAndStop;
        },
        [](bool) {});
    {
        std::unique_lock<std::mutex> locker(me.parkingLock);
        if (!didDequeue) {
            while (me.address) me.parkingCondition.wait(locker);
        }
        me.address = nullptr;
    }
    ParkResult result;
    result.wasUnparked = !didDequeue;
    if (!didDequeue) result.token = me.token;
    return result;
}

template<typename Callback>
void UnparkOneWithCallback(const void* address, const Callback& callback) {
    ThreadData* threadData = nullptr;
    bool timeToBeFair = false;
    DequeueThreads(
        address,
        [&](ThreadData* element, bool passedTimeToBeFair) {
            if (element->address != address) return DequeueResult::Ignore;
            threadData = element;
            timeToBeFair = passedTimeToBeFair;
            return DequeueResult::RemoveAndStop;
        },
        [&](bool mayHaveMoreThreads) {
            UnparkResult result;
            result.didUnparkThread = threadData;
            result.mayHaveMoreThreads = result.didUnparkThread && mayHaveMoreThreads;
            result.timeToBeFair = timeToBeFair;
            const std::intptr_t token = callback(result);
            if (threadData) threadData->token = token;
        });
    if (threadData) Wake(*threadData);
}

unsigned UnparkCount(const void* address, unsigned count) {
    if (!count) return 0;
    std::vector<ThreadData*> threadDatas;
    DequeueThreads(
        address,
        [&](ThreadData* element, bool) {
            if (element->address != address) return DequeueResult::Ignore;
            threadDatas.push_back(element);
            if (threadDatas.size() == count) return DequeueResult::RemoveAndStop;
            return DequeueResult::RemoveAndContinue;
        },
        [](bool) {});
    for (ThreadData* threadData : threadDatas) Wake(*threadData);
    return static_cast<unsigned>(threadDatas.size());
}

constexpr TimeWithDynamicClockType Infinity{std::numeric_limits<double>::infinity(), WTF::ClockType::Wall};

template<typename Word, Word IsHeldBit, Word HasParkedBit>
struct LockAlgorithm {
    static constexpr Word Mask = IsHeldBit | HasParkedBit;
    static constexpr std::intptr_t BargingOpportunity = 0;
    static constexpr std::intptr_t DirectHandoff = 1;

    static void LockSlow(Word& word) {
        std::atomic_ref<Word> lock(word);
        unsigned spinCount = 0;
        for (;;) {
            Word currentValue = lock.load();
            if (!(currentValue & IsHeldBit)) {
                if (lock.compare_exchange_weak(currentValue, static_cast<Word>(currentValue | IsHeldBit))) return;
                continue;
            }
            if (!(currentValue & HasParkedBit) && spinCount < SpinLimit) {
                ++spinCount;
                std::this_thread::yield();
                continue;
            }
            if (!(currentValue & HasParkedBit)) {
                const Word newValue = currentValue | HasParkedBit;
                if (!lock.compare_exchange_weak(currentValue, newValue)) continue;
                currentValue = newValue;
            }
            const ParkResult parkResult = ParkConditionally(
                &word, [&] { return lock.load() == currentValue; }, [] {}, Infinity);
            if (parkResult.wasUnparked && parkResult.token == DirectHandoff) {
                if (!(lock.load() & IsHeldBit)) throw std::logic_error("WTF::Lock: direct handoff of a lock that is not held");
                return;
            }
        }
    }

    static void UnlockSlow(Word& word, bool fair) {
        std::atomic_ref<Word> lock(word);
        for (;;) {
            Word oldValue = lock.load();
            if ((oldValue & Mask) != IsHeldBit && (oldValue & Mask) != Mask) throw std::logic_error("WTF::Lock: invalid value for lock");
            if ((oldValue & Mask) == IsHeldBit) {
                if (lock.compare_exchange_weak(oldValue, static_cast<Word>(oldValue & ~IsHeldBit))) return;
                continue;
            }
            UnparkOneWithCallback(&word, [&](UnparkResult result) -> std::intptr_t {
                if (result.didUnparkThread && (fair || result.timeToBeFair)) return DirectHandoff;
                Word value = lock.load();
                Word newValue;
                do {
                    newValue = value & ~Mask;
                    if (result.mayHaveMoreThreads) newValue |= HasParkedBit;
                } while (!lock.compare_exchange_weak(value, newValue));
                return BargingOpportunity;
            });
            return;
        }
    }

    static void LockWord(Word& word) {
        Word expected = 0;
        if (std::atomic_ref<Word>(word).compare_exchange_weak(expected, IsHeldBit, std::memory_order_acquire)) return;
        LockSlow(word);
    }

    static void UnlockWord(Word& word, bool fair) {
        Word expected = IsHeldBit;
        if (std::atomic_ref<Word>(word).compare_exchange_weak(expected, 0, std::memory_order_release)) return;
        UnlockSlow(word, fair);
    }
};

using DefaultLockAlgorithm = LockAlgorithm<std::uint8_t, 1, 2>;

struct LockHolder {
    explicit LockHolder(Lock& held) : lock(held) { DefaultLockAlgorithm::LockWord(lock.byte); }
    ~LockHolder() { DefaultLockAlgorithm::UnlockWord(lock.byte, false); }
    LockHolder(const LockHolder&) = delete;
    LockHolder& operator=(const LockHolder&) = delete;
    Lock& lock;
};

bool ConditionWaitUntil(Condition& condition, Lock& lock, const TimeWithDynamicClockType& timeout) {
    bool result;
    if (timeout.m_value < _ZNK3WTF24TimeWithDynamicClockType16nowWithSameClockEv(&timeout).m_value) {
        DefaultLockAlgorithm::UnlockWord(lock.byte, false);
        result = false;
    } else {
        std::atomic_ref<std::uint8_t> hasWaiters(condition.hasWaiters);
        result = ParkConditionally(
            &condition.hasWaiters,
            [&] {
                hasWaiters.store(1);
                return true;
            },
            [&] { DefaultLockAlgorithm::UnlockWord(lock.byte, false); },
            timeout).wasUnparked;
    }
    DefaultLockAlgorithm::LockWord(lock.byte);
    return result;
}

void ConditionWait(Condition& condition, Lock& lock) {
    ConditionWaitUntil(condition, lock, Infinity);
}

void ConditionNotifyOne(Condition& condition) {
    std::atomic_ref<std::uint8_t> hasWaiters(condition.hasWaiters);
    if (!hasWaiters.load()) return;
    UnparkOneWithCallback(&condition.hasWaiters, [&](UnparkResult result) -> std::intptr_t {
        if (!result.mayHaveMoreThreads) hasWaiters.store(0);
        return 0;
    });
}

void ConditionNotifyAll(Condition& condition) {
    std::atomic_ref<std::uint8_t> hasWaiters(condition.hasWaiters);
    if (!hasWaiters.load()) return;
    hasWaiters.store(0);
    UnparkCount(&condition.hasWaiters, UINT_MAX);
}

struct WordLockThreadData {
    bool shouldPark{false};
    std::mutex parkingLock;
    std::condition_variable parkingCondition;
    WordLockThreadData* nextInQueue{nullptr};
    WordLockThreadData* queueTail{nullptr};
};

constexpr std::uintptr_t WordIsLockedBit = 1;
constexpr std::uintptr_t WordIsQueueLockedBit = 2;
constexpr std::uintptr_t WordQueueHeadMask = 3;

}  // namespace

extern "C" {

ParkResult APS5_VABI _ZN3WTF10ParkingLot21parkConditionallyImplEPKvRKNS_12ScopedLambdaIFbvEEERKNS3_IFvvEEERKNS_24TimeWithDynamicClockTypeE(const void* address, const ValidationLambda* validation, const BeforeSleepLambda* beforeSleep, const TimeWithDynamicClockType* timeout) {
    const auto& validate = Checked(validation, "ParkingLot::parkConditionally");
    const auto& sleep = Checked(beforeSleep, "ParkingLot::parkConditionally");
    const auto& until = Checked(timeout, "ParkingLot::parkConditionally");
    return ParkConditionally(
        address,
        [&] { return validate.impl(validate.argument); },
        [&] { sleep.impl(sleep.argument); },
        until);
}

UnparkResult APS5_VABI _ZN3WTF10ParkingLot9unparkOneEPKv(const void* address) {
    UnparkResult unparked;
    UnparkOneWithCallback(address, [&](UnparkResult result) -> std::intptr_t {
        unparked = result;
        return 0;
    });
    return unparked;
}

void APS5_VABI _ZN3WTF10ParkingLot13unparkOneImplEPKvRKNS_12ScopedLambdaIFlNS0_12UnparkResultEEEE(const void* address, const UnparkCallbackLambda* callback) {
    const auto& call = Checked(callback, "ParkingLot::unparkOne");
    UnparkOneWithCallback(address, [&](UnparkResult result) { return call.impl(call.argument, result); });
}

unsigned APS5_VABI _ZN3WTF10ParkingLot11unparkCountEPKvj(const void* address, unsigned count) {
    return UnparkCount(address, count);
}

void APS5_VABI _ZN3WTF10ParkingLot9unparkAllEPKv(const void* address) {
    UnparkCount(address, UINT_MAX);
}

void APS5_VABI _ZN3WTF4Lock8lockSlowEv(Lock* self) {
    DefaultLockAlgorithm::LockSlow(Checked(self, "Lock::lockSlow").byte);
}

void APS5_VABI _ZN3WTF4Lock10unlockSlowEv(Lock* self) {
    DefaultLockAlgorithm::UnlockSlow(Checked(self, "Lock::unlockSlow").byte, false);
}

void APS5_VABI _ZN3WTF4Lock13safepointSlowEv(Lock* self) {
    auto& lock = Checked(self, "Lock::safepointSlow");
    DefaultLockAlgorithm::UnlockWord(lock.byte, true);
    DefaultLockAlgorithm::LockWord(lock.byte);
}

void APS5_VABI _ZN3WTF8WordLock8lockSlowEv(WordLock* self) {
    std::atomic_ref<std::uintptr_t> word(Checked(self, "WordLock::lockSlow").word);
    unsigned spinCount = 0;
    for (;;) {
        std::uintptr_t currentWordValue = word.load();
        if (!(currentWordValue & WordIsLockedBit)) {
            if (word.compare_exchange_weak(currentWordValue, currentWordValue | WordIsLockedBit)) return;
        }
        if (!(currentWordValue & ~WordQueueHeadMask) && spinCount < SpinLimit) {
            ++spinCount;
            std::this_thread::yield();
            continue;
        }
        WordLockThreadData me;
        currentWordValue = word.load();
        if ((currentWordValue & WordIsQueueLockedBit) || !(currentWordValue & WordIsLockedBit) || !word.compare_exchange_weak(currentWordValue, currentWordValue | WordIsQueueLockedBit)) {
            std::this_thread::yield();
            continue;
        }
        me.shouldPark = true;
        auto* queueHead = reinterpret_cast<WordLockThreadData*>(currentWordValue & ~WordQueueHeadMask);
        if (queueHead) {
            queueHead->queueTail->nextInQueue = &me;
            queueHead->queueTail = &me;
            currentWordValue = word.load();
            word.store(currentWordValue & ~WordIsQueueLockedBit);
        } else {
            queueHead = &me;
            me.queueTail = &me;
            currentWordValue = word.load();
            std::uintptr_t newWordValue = currentWordValue;
            newWordValue |= reinterpret_cast<std::uintptr_t>(queueHead);
            newWordValue &= ~WordIsQueueLockedBit;
            word.store(newWordValue);
        }
        std::unique_lock<std::mutex> locker(me.parkingLock);
        while (me.shouldPark) me.parkingCondition.wait(locker);
    }
}

void APS5_VABI _ZN3WTF8WordLock10unlockSlowEv(WordLock* self) {
    std::atomic_ref<std::uintptr_t> word(Checked(self, "WordLock::unlockSlow").word);
    for (;;) {
        std::uintptr_t currentWordValue = word.load();
        if (!(currentWordValue & WordIsLockedBit)) throw std::logic_error("WTF::WordLock: unlock of a lock that is not held");
        if (currentWordValue == WordIsLockedBit) {
            if (word.compare_exchange_weak(currentWordValue, 0)) return;
            std::this_thread::yield();
            continue;
        }
        if (currentWordValue & WordIsQueueLockedBit) {
            std::this_thread::yield();
            continue;
        }
        if (word.compare_exchange_weak(currentWordValue, currentWordValue | WordIsQueueLockedBit)) break;
    }
    std::uintptr_t currentWordValue = word.load();
    auto* queueHead = reinterpret_cast<WordLockThreadData*>(currentWordValue & ~WordQueueHeadMask);
    WordLockThreadData* newQueueHead = queueHead->nextInQueue;
    if (newQueueHead) newQueueHead->queueTail = queueHead->queueTail;
    currentWordValue = word.load();
    std::uintptr_t newWordValue = currentWordValue;
    newWordValue &= ~WordIsLockedBit;
    newWordValue &= ~WordIsQueueLockedBit;
    newWordValue &= WordQueueHeadMask;
    newWordValue |= reinterpret_cast<std::uintptr_t>(newQueueHead);
    word.store(newWordValue);
    queueHead->nextInQueue = nullptr;
    queueHead->queueTail = nullptr;
    std::lock_guard<std::mutex> locker(queueHead->parkingLock);
    queueHead->shouldPark = false;
    queueHead->parkingCondition.notify_one();
}

void APS5_VABI _ZN3WTF13ReadWriteLock8readLockEv(ReadWriteLock* self) {
    auto& rw = Checked(self, "ReadWriteLock::readLock");
    LockHolder locker(rw.lock);
    while (rw.isWriteLocked || rw.numWaitingWriters) ConditionWait(rw.condition, rw.lock);
    ++rw.numReaders;
}

void APS5_VABI _ZN3WTF13ReadWriteLock10readUnlockEv(ReadWriteLock* self) {
    auto& rw = Checked(self, "ReadWriteLock::readUnlock");
    LockHolder locker(rw.lock);
    --rw.numReaders;
    if (!rw.numReaders) ConditionNotifyAll(rw.condition);
}

void APS5_VABI _ZN3WTF13ReadWriteLock9writeLockEv(ReadWriteLock* self) {
    auto& rw = Checked(self, "ReadWriteLock::writeLock");
    LockHolder locker(rw.lock);
    while (rw.isWriteLocked || rw.numReaders) {
        ++rw.numWaitingWriters;
        ConditionWait(rw.condition, rw.lock);
        --rw.numWaitingWriters;
    }
    rw.isWriteLocked = true;
}

void APS5_VABI _ZN3WTF13ReadWriteLock11writeUnlockEv(ReadWriteLock* self) {
    auto& rw = Checked(self, "ReadWriteLock::writeUnlock");
    LockHolder locker(rw.lock);
    rw.isWriteLocked = false;
    ConditionNotifyAll(rw.condition);
}

void APS5_VABI _ZN3WTF15BinarySemaphore6signalEv(BinarySemaphore* self) {
    auto& semaphore = Checked(self, "BinarySemaphore::signal");
    LockHolder locker(semaphore.lock);
    semaphore.isSet = true;
    ConditionNotifyOne(semaphore.condition);
}

bool APS5_VABI _ZN3WTF15BinarySemaphore9waitUntilERKNS_24TimeWithDynamicClockTypeE(BinarySemaphore* self, const TimeWithDynamicClockType* absoluteTime) {
    auto& semaphore = Checked(self, "BinarySemaphore::waitUntil");
    const auto& timeout = Checked(absoluteTime, "BinarySemaphore::waitUntil");
    LockHolder locker(semaphore.lock);
    bool satisfied = true;
    while (!semaphore.isSet) {
        if (!ConditionWaitUntil(semaphore.condition, semaphore.lock, timeout)) {
            satisfied = semaphore.isSet;
            break;
        }
    }
    if (satisfied) semaphore.isSet = false;
    return satisfied;
}

}
