// Derived from neonmodder123/PS5-WebKit-11.x/Source/WTF/wtf/ParallelJobsGeneric.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2011 University of Szeged
 * Copyright (C) 2011 Gabor Loki <loki@webkit.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceNKWebKit/include/ParallelEnvironmentExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {

using NKWebKit::WtfAbi::ParallelEnvironment;
using NKWebKit::WtfAbi::ParallelEnvironmentFunction;
using NKWebKit::WtfAbi::ParallelEnvironmentThread;
using NKWebKit::WtfAbi::ParallelEnvironmentThreadVector;

struct ParallelEnvironmentPrefix {
    ParallelEnvironmentFunction threadFunction;
    std::size_t sizeOfParameter;
    int numberOfJobs;
};

static_assert(std::is_trivially_copyable_v<ParallelEnvironment>);
static_assert(std::is_trivially_copyable_v<ParallelEnvironmentPrefix>);
static_assert(std::is_trivially_copyable_v<ParallelEnvironmentThreadVector>);
static_assert(offsetof(ParallelEnvironmentPrefix, threadFunction) == offsetof(ParallelEnvironment, threadFunction));
static_assert(offsetof(ParallelEnvironmentPrefix, sizeOfParameter) == offsetof(ParallelEnvironment, sizeOfParameter));
static_assert(offsetof(ParallelEnvironmentPrefix, numberOfJobs) == offsetof(ParallelEnvironment, numberOfJobs));
constexpr std::size_t kPrefixWriteBytes = offsetof(ParallelEnvironment, numberOfJobs) + sizeof(int);

void RequireReadableGuestBytes(const void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(pointer, size);
    if (!GuestReadable_nid_no_patch(pointer, size))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

struct Worker : ParallelEnvironmentThread {
    std::mutex lock;
    std::condition_variable condition;
    bool hasThread = false;
    bool running = false;
    ParallelEnvironment* parent = nullptr;
    ParallelEnvironmentFunction threadFunction = nullptr;
    void* parameters = nullptr;

    Worker() : ParallelEnvironmentThread{1} {}
};

std::vector<Worker*>& ThreadPool() {
    static std::vector<Worker*>* const pool = new std::vector<Worker*>;
    return *pool;
}

std::mutex& ThreadPoolLock() {
    static std::mutex* const lock = new std::mutex;
    return *lock;
}

void* APS5_VABI RunWorker(void* argument) {
    auto* worker = static_cast<Worker*>(argument);
    std::unique_lock lock(worker->lock);
    for (;;) {
        if (worker->running) {
            worker->threadFunction(worker->parameters);
            worker->running = false;
            worker->parent = nullptr;
            worker->condition.notify_one();
        }
        worker->condition.wait(lock);
    }
}

bool TryLockFor(Worker& worker, ParallelEnvironment* parent) {
    std::unique_lock lock(worker.lock, std::try_to_lock);
    if (!lock.owns_lock()) return false;
    if (worker.parent) return false;
    if (!worker.hasThread) {
        Pthread thread = nullptr;
        if (const int error = pthread_create_nid_postfix(&thread, nullptr, &RunWorker, &worker))
            throw std::runtime_error("WTF::ParallelEnvironment: pthread_create failed with " + std::to_string(error));
        worker.hasThread = true;
    }
    worker.parent = parent;
    return true;
}

struct ConstructGuard {
    std::vector<Worker*> taken;
    unsigned retainedCount{0};
    ParallelEnvironmentThread** buffer{nullptr};
    bool armed{true};

    ~ConstructGuard() {
        if (!armed) return;
        for (unsigned index = 0; index < retainedCount; ++index) --taken[index]->refCount;
        for (Worker* worker : taken) {
            std::lock_guard lock(worker->lock);
            worker->parent = nullptr;
        }
        if (buffer) _ZN3WTF8fastFreeEPv(buffer);
    }

    ConstructGuard() = default;
    ConstructGuard(const ConstructGuard&) = delete;
    ConstructGuard& operator=(const ConstructGuard&) = delete;
};

void Construct(ParallelEnvironment* self, ParallelEnvironmentFunction threadFunction, std::size_t sizeOfParameter, int requestedJobNumber, const char* function) {
    if (!self) throw std::invalid_argument("null WTF::ParallelEnvironment");
    const GuestArena::HostWrite writeGuard(self, sizeof(ParallelEnvironment));
    if (!writeGuard.Open() || !GuestWritable_nid_no_patch(self, sizeof(ParallelEnvironment)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const int maxNumberOfCores = _ZN3WTF22numberOfProcessorCoresEv();
    if (!requestedJobNumber || requestedJobNumber > maxNumberOfCores) requestedJobNumber = maxNumberOfCores;
    const int maxNumberOfNewThreads = requestedJobNumber - 1;
    ConstructGuard guard;
    if (maxNumberOfCores > 0) guard.taken.reserve(static_cast<std::size_t>(maxNumberOfCores));
    {
        std::lock_guard poolLock(ThreadPoolLock());
        std::vector<Worker*>& pool = ThreadPool();
        for (int index = 0; index < maxNumberOfCores && guard.taken.size() < static_cast<unsigned>(maxNumberOfNewThreads); ++index) {
            if (pool.size() < static_cast<unsigned>(index) + 1U) pool.push_back(new Worker);
            if (TryLockFor(*pool[index], self)) guard.taken.push_back(pool[index]);
        }
    }
    const auto count = static_cast<unsigned>(guard.taken.size());
    if (count) {
        guard.buffer = static_cast<ParallelEnvironmentThread**>(_ZN3WTF10fastMallocEm(count * sizeof(ParallelEnvironmentThread*)));
        for (unsigned index = 0; index < count; ++index) {
            ++guard.taken[index]->refCount;
            guard.retainedCount = index + 1;
            guard.buffer[index] = guard.taken[index];
        }
    }
    const ParallelEnvironmentPrefix prefix{threadFunction, sizeOfParameter, static_cast<int>(count) + 1};
    const ParallelEnvironmentThreadVector threads{guard.buffer, count, count};
    if (!GuestCopyOut_nid_no_patch(self, &prefix, kPrefixWriteBytes))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (!GuestCopyOut_nid_no_patch(reinterpret_cast<unsigned char*>(self) + offsetof(ParallelEnvironment, threads), &threads, sizeof(threads)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    guard.armed = false;
}

void Execute(ParallelEnvironment* self, void* parameters, const char* function) {
    if (!self) throw std::invalid_argument("null WTF::ParallelEnvironment");
    ParallelEnvironment local{};
    if (!GuestCopyIn_nid_no_patch(&local, self, sizeof(local)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (!local.threadFunction) throw std::invalid_argument("WTF::ParallelEnvironment::execute: null thread function");

    std::vector<Worker*> workers;
    if (local.threads.size) {
        if (!local.threads.buffer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        if (local.threads.size > std::numeric_limits<std::size_t>::max() / sizeof(ParallelEnvironmentThread*))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        const std::size_t totalBytes = static_cast<std::size_t>(local.threads.size) * sizeof(ParallelEnvironmentThread*);
        const auto baseAddress = reinterpret_cast<std::uintptr_t>(local.threads.buffer);
        if (totalBytes > std::numeric_limits<std::uintptr_t>::max() - baseAddress)
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        RequireReadableGuestBytes(local.threads.buffer, totalBytes, function);
        std::vector<ParallelEnvironmentThread*> rawThreads(local.threads.size);
        if (!GuestCopyIn_nid_no_patch(rawThreads.data(), local.threads.buffer, totalBytes))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        workers.reserve(local.threads.size);
        std::lock_guard poolLock(ThreadPoolLock());
        const std::vector<Worker*>& pool = ThreadPool();
        for (ParallelEnvironmentThread* entry : rawThreads) {
            if (!entry) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
            Worker* matched = nullptr;
            for (Worker* candidate : pool) {
                if (static_cast<ParallelEnvironmentThread*>(candidate) == entry) {
                    matched = candidate;
                    break;
                }
            }
            if (!matched) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
            for (Worker* existing : workers) {
                if (existing == matched) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
            }
            workers.push_back(matched);
        }
    }

    auto currentAddress = reinterpret_cast<std::uintptr_t>(parameters);
    for (Worker* worker : workers) {
        {
            std::lock_guard lock(worker->lock);
            worker->threadFunction = local.threadFunction;
            worker->parameters = reinterpret_cast<void*>(currentAddress);
            worker->running = true;
        }
        worker->condition.notify_one();
        currentAddress += local.sizeOfParameter;
    }
    local.threadFunction(reinterpret_cast<void*>(currentAddress));
    for (Worker* worker : workers) {
        std::unique_lock lock(worker->lock);
        worker->condition.wait(lock, [worker] { return !worker->running; });
    }
}

}  // namespace

extern "C" {

void APS5_VABI _ZN3WTF19ParallelEnvironmentC1EPFvPvEmi(
    ParallelEnvironment* self, ParallelEnvironmentFunction threadFunction, std::size_t sizeOfParameter, int requestedJobNumber) {
    Construct(self, threadFunction, sizeOfParameter, requestedJobNumber, __func__);
}

void APS5_VABI _ZN3WTF19ParallelEnvironmentC2EPFvPvEmi(
    ParallelEnvironment* self, ParallelEnvironmentFunction threadFunction, std::size_t sizeOfParameter, int requestedJobNumber) {
    Construct(self, threadFunction, sizeOfParameter, requestedJobNumber, __func__);
}

void APS5_VABI _ZN3WTF19ParallelEnvironment7executeEPv(ParallelEnvironment* self, void* parameters) {
    Execute(self, parameters, __func__);
}

}
