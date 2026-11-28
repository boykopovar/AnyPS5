//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// https://github.com/llvm/llvm-project/blob/6009708b4367171ccdbf4b5905cb6a803753fe18/libcxx/src/thread.cpp

#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/LibcxxThreadForwards.hpp"

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

struct GuestThreadStruct {
    void* implementation;
};
static_assert(sizeof(GuestThreadStruct) == sizeof(void*));
static_assert(sizeof(PthreadKey) == 4);

struct ThreadState {
    std::vector<void*> asyncStates;
    std::vector<std::pair<void*, void*>> notifications;
};

struct StateRegistry {
    std::mutex mutex;
    std::unordered_set<ThreadState*> states;
};

StateRegistry& Registry() {
    static auto* registry = new StateRegistry;
    return *registry;
}

std::mutex keyMutex;
std::atomic<const LibcxxThreadForwards_nid_no_patch*> registeredForwards{nullptr};
PthreadKey threadKey = 0;

void APS5_VABI AtThreadExit(void* object) {
    _ZNSt3__115__thread_structD1Ev_nid_postfix(object);
    _ZdlPv_nid_postfix(object);
}

}

extern "C" {

void RegisterLibcxxThreadForwards_nid_no_patch(const LibcxxThreadForwards_nid_no_patch* forwards) {
    std::lock_guard lock(keyMutex);
    registeredForwards.store(forwards, std::memory_order_release);
    threadKey = 0;
}

void APS5_VABI _ZNSt3__115__thread_structC1Ev_nid_postfix(void* self) {
    if (self == nullptr) throw std::invalid_argument("libc++ thread struct: null object");
    auto state = std::make_unique<ThreadState>();
    {
        auto& registry = Registry();
        std::lock_guard lock(registry.mutex);
        registry.states.insert(state.get());
    }
    const GuestThreadStruct guest{state.get()};
    if (!GuestCopyOut_nid_no_patch(self, &guest, sizeof(guest))) {
        auto& registry = Registry();
        std::lock_guard lock(registry.mutex);
        registry.states.erase(state.get());
        throw std::invalid_argument("libc++ thread struct: unwritable object");
    }
    state.release();
}

void APS5_VABI _ZNSt3__115__thread_structD1Ev_nid_postfix(void* self) {
    GuestThreadStruct guest{};
    if (!GuestCopyIn_nid_no_patch(&guest, self, sizeof(guest)))
        throw std::invalid_argument("libc++ thread struct: unreadable object");
    auto* state = static_cast<ThreadState*>(guest.implementation);
    {
        auto& registry = Registry();
        std::lock_guard lock(registry.mutex);
        if (registry.states.erase(state) == 0)
            throw std::invalid_argument("libc++ thread struct: unknown state");
    }
    const GuestThreadStruct empty{};
    if (!GuestCopyOut_nid_no_patch(self, &empty, sizeof(empty))) {
        auto& registry = Registry();
        std::lock_guard lock(registry.mutex);
        registry.states.insert(state);
        throw std::invalid_argument("libc++ thread struct: unwritable object");
    }
    delete state;
}

PthreadKey* APS5_VABI _ZNSt3__119__thread_local_dataEv_nid_postfix() {
    std::lock_guard lock(keyMutex);
    const auto* forwards = registeredForwards.load(std::memory_order_acquire);
    if (forwards == nullptr) throw std::runtime_error("libc++ thread-local data: libkernel pthread calls are not registered");
    if (threadKey == 0) {
        PthreadKey created = 0;
        const int error = forwards->keyCreate(&created, reinterpret_cast<pthread_key_destructor_func_t>(AtThreadExit));
        if (error != 0) throw std::system_error(error, std::generic_category(), "libc++ thread-local data: key creation failed");
        threadKey = created;
    }
    return &threadKey;
}

}
