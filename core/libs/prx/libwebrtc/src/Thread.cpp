/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Copyright (c) 2011, The WebRTC project authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 *   * Neither the name of Google nor the names of its contributors may
 *     be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/rtc_base/thread.cc */

#include "prx/libwebrtc/libwebrtc.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <algorithm>
#include <cerrno>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kStopOffset = 0x64;
constexpr std::size_t kSocketServerOffset = 0x68;
constexpr int kForever = -1;
constexpr std::int64_t kPlusInfinityMicroseconds = std::numeric_limits<std::int64_t>::max();
constexpr std::size_t kSocketServerSetMessageQueueSlot = 3;
constexpr std::size_t kSocketServerWaitSlot = 4;
constexpr std::size_t kSocketServerWakeUpSlot = 5;
constexpr std::size_t kPostTaskSlot = 1;
constexpr std::size_t kDeletingDestructorSlot = 4;
constexpr std::size_t kIsQuittingSlot = 6;
constexpr std::size_t kRestartSlot = 7;
constexpr std::size_t kStopSlot = 10;
constexpr std::size_t kRunSlot = 11;
constexpr std::uint8_t kRelocate = 0;
constexpr std::uint8_t kDispose = 1;
constexpr std::uint8_t kEmptyTraits = 0;
constexpr std::uint8_t kEmptyLocation = 0;

using SetMessageQueueFunction = void (APS5_VABI*)(void* socketServer, void* thread);
using WaitFunction = bool (APS5_VABI*)(void* socketServer, std::int64_t maxWaitMicroseconds, bool processIo);
using WakeUpFunction = void (APS5_VABI*)(void* socketServer);
using ThreadFunction = void (APS5_VABI*)(void* thread);
using IsQuittingFunction = bool (APS5_VABI*)(void* thread);
using PostTaskFunction = void (APS5_VABI*)(void* thread, WebRtcAnyInvocable* task, const void* traits, const void* location);

void APS5_VABI EmptyManager(std::uint8_t, WebRtcAnyInvocableState*, WebRtcAnyInvocableState*) {
}

struct Task {
    WebRtcAnyInvocable invocable{{}, &EmptyManager, nullptr};

    Task() = default;
    explicit Task(WebRtcAnyInvocable* source) {
        MoveFrom(*source);
    }
    Task(Task&& other) noexcept {
        MoveFrom(other.invocable);
    }
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            invocable.manager(kDispose, &invocable.state, &invocable.state);
            MoveFrom(other.invocable);
        }
        return *this;
    }
    ~Task() {
        invocable.manager(kDispose, &invocable.state, &invocable.state);
    }

    void MoveFrom(WebRtcAnyInvocable& source) {
        source.manager(kRelocate, &source.state, &invocable.state);
        invocable.manager = source.manager;
        invocable.invoker = source.invoker;
        source.manager = &EmptyManager;
        source.invoker = nullptr;
    }

    explicit operator bool() const {
        return invocable.invoker != nullptr;
    }
};

struct DelayedTask {
    std::int64_t runTimeMs;
    std::uint32_t messageNumber;
    Task task;
};

struct ThreadState {
    std::mutex mutex;
    std::deque<Task> messages;
    std::vector<DelayedTask> delayed;
    std::uint32_t delayedNextNumber = 0;
    bool destroyed = false;
    Pthread thread{};
    bool running = false;
    WebRtcCurrentTaskQueueSetter registration{};
    bool registered = false;
};

std::mutex registryMutex;
std::unordered_map<const void*, std::unique_ptr<ThreadState>> registry;

ThreadState& StateOf(const void* thread, const char* caller) {
    std::lock_guard lock(registryMutex);
    const auto found = registry.find(thread);
    if (found == registry.end()) throw std::invalid_argument(std::string(caller) + ": not a thread made by rtc::Thread::Thread");
    return *found->second;
}

template<typename T>
T ReadField(const void* object, std::size_t offset, const char* message) {
    T value{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, static_cast<const std::uint8_t*>(object) + offset, sizeof(value));
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(message);
    return value;
}

template<typename T>
void WriteField(void* object, std::size_t offset, const T& value, const char* message) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(static_cast<std::uint8_t*>(object) + offset, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(message);
}

void* SocketServerOf(const void* thread) {
    return ReadField<void*>(thread, kSocketServerOffset, "rtc::Thread: unreadable socket server");
}

void* SocketServerSlot(void* socketServer, std::size_t slot) {
    void** vtable = ReadField<void**>(socketServer, 0, "rtc::Thread: unreadable socket server");
    return ReadField<void*>(vtable, slot * sizeof(void*), "rtc::Thread: unreadable socket server");
}

void* SlotOf(void* thread, std::size_t slot) {
    void** vtable = ReadField<void**>(thread, 0, "rtc::Thread: unreadable thread");
    return ReadField<void*>(vtable, slot * sizeof(void*), "rtc::Thread: unreadable thread");
}

bool IsQuitting(void* thread) {
    return reinterpret_cast<IsQuittingFunction>(SlotOf(thread, kIsQuittingSlot))(thread);
}

void WakeUpSocketServer(void* thread) {
    void* socketServer = SocketServerOf(thread);
    reinterpret_cast<WakeUpFunction>(SocketServerSlot(socketServer, kSocketServerWakeUpSlot))(socketServer);
}

bool Later(const DelayedTask& first, const DelayedTask& second) {
    return second.runTimeMs < first.runTimeMs || (second.runTimeMs == first.runTimeMs && second.messageNumber < first.messageNumber);
}

Task Get(void* thread, ThreadState& state, int cmsWait) {
    const std::int64_t cmsTotal = cmsWait;
    std::int64_t cmsElapsed = 0;
    const std::int64_t msStart = _ZN3rtc10TimeMillisEv_nid_postfix();
    std::int64_t msCurrent = msStart;
    while (true) {
        std::int64_t cmsDelayNext = kForever;
        {
            std::lock_guard lock(state.mutex);
            while (!state.delayed.empty()) {
                if (msCurrent < state.delayed.front().runTimeMs) {
                    cmsDelayNext = state.delayed.front().runTimeMs - msCurrent;
                    break;
                }
                std::pop_heap(state.delayed.begin(), state.delayed.end(), Later);
                state.messages.push_back(std::move(state.delayed.back().task));
                state.delayed.pop_back();
            }
            if (!state.messages.empty()) {
                Task task = std::move(state.messages.front());
                state.messages.pop_front();
                return task;
            }
        }
        if (IsQuitting(thread)) break;
        std::int64_t cmsNext;
        if (cmsWait == kForever) {
            cmsNext = cmsDelayNext;
        } else {
            cmsNext = std::max<std::int64_t>(0, cmsTotal - cmsElapsed);
            if (cmsDelayNext != kForever && cmsDelayNext < cmsNext) cmsNext = cmsDelayNext;
        }
        void* socketServer = SocketServerOf(thread);
        const std::int64_t wait = cmsNext == kForever ? kPlusInfinityMicroseconds : cmsNext * 1000;
        if (!reinterpret_cast<WaitFunction>(SocketServerSlot(socketServer, kSocketServerWaitSlot))(socketServer, wait, true)) return Task();
        msCurrent = _ZN3rtc10TimeMillisEv_nid_postfix();
        cmsElapsed = msCurrent - msStart;
        if (cmsWait != kForever && cmsElapsed >= cmsWait) return Task();
    }
    return Task();
}

void DoDestroy(void* thread, ThreadState& state) {
    if (state.destroyed) return;
    state.destroyed = true;
    if (void* socketServer = SocketServerOf(thread))
        reinterpret_cast<SetMessageQueueFunction>(SocketServerSlot(socketServer, kSocketServerSetMessageQueueSlot))(socketServer, nullptr);
    WebRtcCurrentTaskQueueSetter setCurrent{};
    _ZN6webrtc13TaskQueueBase22CurrentTaskQueueSetterC1EPS0__nid_postfix(&setCurrent, static_cast<WebRtcTaskQueueBase*>(thread));
    std::deque<Task> messages;
    std::vector<DelayedTask> delayed;
    {
        std::lock_guard lock(state.mutex);
        messages.swap(state.messages);
        delayed.swap(state.delayed);
    }
    messages.clear();
    delayed.clear();
    _ZN6webrtc13TaskQueueBase22CurrentTaskQueueSetterD1Ev_nid_postfix(&setCurrent);
}

void Stop(void* thread, ThreadState& state) {
    _ZN3rtc6Thread4QuitEv_nid_postfix(thread);
    if (!state.running) return;
    if (pthread_join_nid_postfix(state.thread, nullptr) != 0) throw std::runtime_error("rtc::Thread::Join: pthread_join failed");
    state.running = false;
}

void* APS5_VABI PreRun(void* thread) {
    WebRtcThreadManager* manager = _ZN3rtc13ThreadManager8InstanceEv_nid_postfix();
    _ZN3rtc13ThreadManager16SetCurrentThreadEPNS_6ThreadE_nid_postfix(manager, thread);
    reinterpret_cast<ThreadFunction>(SlotOf(thread, kRunSlot))(thread);
    _ZN3rtc13ThreadManager16SetCurrentThreadEPNS_6ThreadE_nid_postfix(manager, nullptr);
    return nullptr;
}

void APS5_VABI HostFunctionManager(std::uint8_t operation, WebRtcAnyInvocableState* from, WebRtcAnyInvocableState* to) {
    std::function<void()>* function = nullptr;
    std::memcpy(&function, from->storage, sizeof(function));
    if (operation == kRelocate) std::memcpy(to->storage, &function, sizeof(function));
    else delete function;
}

void APS5_VABI HostFunctionInvoker(WebRtcAnyInvocableState* state) {
    std::function<void()>* function = nullptr;
    std::memcpy(&function, state->storage, sizeof(function));
    (*function)();
}

void APS5_VABI ThreadDeletingDestructor(void* thread) {
    _ZN3rtc6ThreadD2Ev_nid_postfix(thread);
    _ZdlPv_nid_postfix(thread);
}

}  // namespace

extern "C" {

void APS5_VABI _ZN3rtc6ThreadC1EPNS_12SocketServerE_nid_postfix(void* thread, void* socketServer) {
    if (!socketServer) throw std::invalid_argument("rtc::Thread::Thread: null socket server");
    WriteField<void* const*>(thread, 0, WebRtcThreadVtable_nid_no_patch(), "rtc::Thread::Thread: unwritable thread");
    WriteField<std::uint32_t>(thread, kStopOffset, 0, "rtc::Thread::Thread: unwritable thread");
    WriteField<void*>(thread, kSocketServerOffset, socketServer, "rtc::Thread::Thread: unwritable thread");
    std::unique_ptr<ThreadState> state = std::make_unique<ThreadState>();
    {
        std::lock_guard lock(registryMutex);
        std::swap(registry[thread], state);
    }
    reinterpret_cast<SetMessageQueueFunction>(SocketServerSlot(socketServer, kSocketServerSetMessageQueueSlot))(socketServer, thread);
}

void APS5_VABI _ZN3rtc6ThreadD2Ev_nid_postfix(void* thread) {
    ThreadState& state = StateOf(thread, "rtc::Thread::~Thread");
    WriteField<void* const*>(thread, 0, WebRtcThreadVtable_nid_no_patch(), "rtc::Thread::~Thread: unwritable thread");
    Stop(thread, state);
    DoDestroy(thread, state);
    std::unique_ptr<ThreadState> owner;
    std::lock_guard lock(registryMutex);
    const auto found = registry.find(thread);
    owner = std::move(found->second);
    registry.erase(found);
}

void APS5_VABI _ZN3rtc6Thread6DoInitEv_nid_postfix(void* thread) {
    StateOf(thread, "rtc::Thread::DoInit");
}

void APS5_VABI _ZN3rtc6Thread9DoDestroyEv_nid_postfix(void* thread) {
    DoDestroy(thread, StateOf(thread, "rtc::Thread::DoDestroy"));
}

bool APS5_VABI _ZN3rtc6Thread15ProcessMessagesEi_nid_postfix(void* thread, int cmsLoop) {
    ThreadState& state = StateOf(thread, "rtc::Thread::ProcessMessages");
    const std::int64_t msEnd = cmsLoop == kForever ? 0 : _ZN3rtc10TimeMillisEv_nid_postfix() + cmsLoop;
    int cmsNext = cmsLoop;
    while (true) {
        Task task = Get(thread, state, cmsNext);
        if (!task) return !IsQuitting(thread);
        {
            Task dispatched(std::move(task));
            dispatched.invocable.invoker(&dispatched.invocable.state);
        }
        if (cmsLoop != kForever) {
            cmsNext = static_cast<int>(msEnd - _ZN3rtc10TimeMillisEv_nid_postfix());
            if (cmsNext < 0) return true;
        }
    }
}

void APS5_VABI _ZN3rtc6Thread12PostTaskImplEN4absl12AnyInvocableIFvvOEEERKN6webrtc13TaskQueueBase14PostTaskTraitsERKNS5_8LocationE_nid_postfix(
    void* thread, WebRtcAnyInvocable* task, const void*, const void*) {
    ThreadState& state = StateOf(thread, "rtc::Thread::PostTaskImpl");
    if (!task) throw std::invalid_argument("rtc::Thread::PostTaskImpl: null task");
    if (IsQuitting(thread)) return;
    {
        std::lock_guard lock(state.mutex);
        state.messages.emplace_back(task);
    }
    WakeUpSocketServer(thread);
}

void APS5_VABI _ZN3rtc6Thread19PostDelayedTaskImplEN4absl12AnyInvocableIFvvOEEEN6webrtc9TimeDeltaERKNS5_13TaskQueueBase21PostDelayedTaskTraitsERKNS5_8LocationE_nid_postfix(
    void* thread, WebRtcAnyInvocable* task, std::int64_t delayMicroseconds, const void*, const void*) {
    ThreadState& state = StateOf(thread, "rtc::Thread::PostDelayedTaskImpl");
    if (!task) throw std::invalid_argument("rtc::Thread::PostDelayedTaskImpl: null task");
    if (delayMicroseconds > std::numeric_limits<std::int64_t>::max() - 999) throw std::invalid_argument("rtc::Thread::PostDelayedTaskImpl: delay does not fit in int milliseconds");
    const std::int64_t delayMs = (delayMicroseconds + 999) / 1000;
    if (delayMs < std::numeric_limits<int>::min() || delayMs > std::numeric_limits<int>::max())
        throw std::invalid_argument("rtc::Thread::PostDelayedTaskImpl: delay does not fit in int milliseconds");
    if (IsQuitting(thread)) return;
    const std::int64_t runTimeMs = _ZN3rtc10TimeMillisEv_nid_postfix() + delayMs;
    {
        std::lock_guard lock(state.mutex);
        state.delayed.push_back(DelayedTask{runTimeMs, state.delayedNextNumber, Task(task)});
        std::push_heap(state.delayed.begin(), state.delayed.end(), Later);
        ++state.delayedNextNumber;
    }
    WakeUpSocketServer(thread);
}

int APS5_VABI _ZN3rtc6Thread8GetDelayEv_nid_postfix(void* thread) {
    ThreadState& state = StateOf(thread, "rtc::Thread::GetDelay");
    std::lock_guard lock(state.mutex);
    if (!state.messages.empty()) return 0;
    if (!state.delayed.empty()) {
        const int delay = static_cast<int>(state.delayed.front().runTimeMs - _ZN3rtc10TimeMillisEv_nid_postfix());
        return delay < 0 ? 0 : delay;
    }
    return kForever;
}

bool APS5_VABI _ZN3rtc6Thread5StartEv_nid_postfix(void* thread) {
    ThreadState& state = StateOf(thread, "rtc::Thread::Start");
    if (state.running) return false;
    reinterpret_cast<ThreadFunction>(SlotOf(thread, kRestartSlot))(thread);
    _ZN3rtc13ThreadManager8InstanceEv_nid_postfix();
    if (pthread_create_nid_postfix(&state.thread, nullptr, &PreRun, thread) != 0) return false;
    state.running = true;
    return true;
}

void APS5_VABI _ZN3rtc6Thread3RunEv_nid_postfix(void* thread) {
    _ZN3rtc6Thread15ProcessMessagesEi_nid_postfix(thread, kForever);
}

void APS5_VABI _ZN3rtc6Thread4StopEv_nid_postfix(void* thread) {
    Stop(thread, StateOf(thread, "rtc::Thread::Stop"));
}

void APS5_VABI _ZN3rtc6Thread16BlockingCallImplENS_12FunctionViewIFvvEEERKN6webrtc8LocationE_nid_postfix(
    void* thread, void* context, void (APS5_VABI* call)(void* context), const void*) {
    if (IsQuitting(thread)) return;
    if (_ZNK3rtc6Thread9IsCurrentEv_nid_postfix(thread)) {
        call(context);
        return;
    }
    struct Done {
        std::mutex mutex;
        std::condition_variable condition;
        bool set = false;
    };
    const auto done = std::make_shared<Done>();
    {
        std::shared_ptr<void> cleanup(nullptr, [done](void*) {
            {
                std::lock_guard lock(done->mutex);
                done->set = true;
            }
            done->condition.notify_all();
        });
        Task task;
        auto* function = new std::function<void()>([context, call, cleanup = std::move(cleanup)] { call(context); });
        std::memcpy(task.invocable.state.storage, &function, sizeof(function));
        task.invocable.manager = &HostFunctionManager;
        task.invocable.invoker = &HostFunctionInvoker;
        reinterpret_cast<PostTaskFunction>(SlotOf(thread, kPostTaskSlot))(thread, &task.invocable, &kEmptyTraits, &kEmptyLocation);
    }
    std::unique_lock lock(done->mutex);
    done->condition.wait(lock, [&] { return done->set; });
}

void APS5_VABI _ZN3rtc6Thread6DeleteEv_nid_postfix(void* thread) {
    reinterpret_cast<ThreadFunction>(SlotOf(thread, kStopSlot))(thread);
    reinterpret_cast<ThreadFunction>(SlotOf(thread, kDeletingDestructorSlot))(thread);
}

bool APS5_VABI _ZN3rtc6Thread30IsProcessingMessagesForTestingEv_nid_postfix(void* thread) {
    return !IsQuitting(thread);
}

void APS5_VABI _ZN3rtc13ThreadManager16SetCurrentThreadEPNS_6ThreadE_nid_postfix(WebRtcThreadManager* manager, void* thread) {
    WebRtcThreadManager hostManager{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&hostManager, manager, sizeof(hostManager));
    errno = savedErrno;
    if (!copied) throw std::invalid_argument("rtc::ThreadManager::SetCurrentThread: unreadable manager");
    if (thread) {
        ThreadState& state = StateOf(thread, "rtc::ThreadManager::SetCurrentThread");
        WebRtcCurrentTaskQueueSetter registration{};
        _ZN6webrtc13TaskQueueBase22CurrentTaskQueueSetterC1EPS0__nid_postfix(&registration, static_cast<WebRtcTaskQueueBase*>(thread));
        if (state.registered) _ZN6webrtc13TaskQueueBase22CurrentTaskQueueSetterD1Ev_nid_postfix(&state.registration);
        state.registration = registration;
        state.registered = true;
    } else if (void* current = _ZN3rtc13ThreadManager13CurrentThreadEv_nid_postfix(manager)) {
        ThreadState& state = StateOf(current, "rtc::ThreadManager::SetCurrentThread");
        if (state.registered) _ZN6webrtc13TaskQueueBase22CurrentTaskQueueSetterD1Ev_nid_postfix(&state.registration);
        state.registered = false;
    }
    if (pthread_setspecific_nid_postfix(hostManager.key, thread) != 0) throw std::runtime_error("rtc::ThreadManager::SetCurrentThread: pthread_setspecific failed");
}

void* const* APS5_VABI WebRtcThreadVtable_nid_no_patch() {
    static void* const vtable[] = {
        nullptr,
        nullptr,
        reinterpret_cast<void*>(&_ZN3rtc6Thread6DeleteEv_nid_postfix),
        reinterpret_cast<void*>(&_ZN3rtc6Thread12PostTaskImplEN4absl12AnyInvocableIFvvOEEERKN6webrtc13TaskQueueBase14PostTaskTraitsERKNS5_8LocationE_nid_postfix),
        reinterpret_cast<void*>(&_ZN3rtc6Thread19PostDelayedTaskImplEN4absl12AnyInvocableIFvvOEEEN6webrtc9TimeDeltaERKNS5_13TaskQueueBase21PostDelayedTaskTraitsERKNS5_8LocationE_nid_postfix),
        reinterpret_cast<void*>(&_ZN3rtc6ThreadD2Ev_nid_postfix),
        reinterpret_cast<void*>(&ThreadDeletingDestructor),
        reinterpret_cast<void*>(&_ZN3rtc6Thread4QuitEv_nid_postfix),
        reinterpret_cast<void*>(&_ZN3rtc6Thread10IsQuittingEv_nid_postfix),
        reinterpret_cast<void*>(&_ZN3rtc6Thread7RestartEv_nid_postfix),
        reinterpret_cast<void*>(&_ZN3rtc6Thread30IsProcessingMessagesForTestingEv_nid_postfix),
        reinterpret_cast<void*>(&_ZN3rtc6Thread8GetDelayEv_nid_postfix),
        reinterpret_cast<void*>(&_ZN3rtc6Thread4StopEv_nid_postfix),
        reinterpret_cast<void*>(&_ZN3rtc6Thread3RunEv_nid_postfix),
        reinterpret_cast<void*>(&_ZN3rtc6Thread16BlockingCallImplENS_12FunctionViewIFvvEEERKN6webrtc8LocationE_nid_postfix),
    };
    return vtable + 2;
}

}
