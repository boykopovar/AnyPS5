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
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceNKWebKit/include/ParallelEnvironmentExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using NKWebKit::WtfAbi::ParallelEnvironment;
using NKWebKit::WtfAbi::ParallelEnvironmentFunction;
using NKWebKit::WtfAbi::ParallelEnvironmentThread;

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

Worker& WorkerOf(ParallelEnvironmentThread* shared) {
    return *static_cast<Worker*>(shared);
}

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

void Construct(ParallelEnvironment* self, ParallelEnvironmentFunction threadFunction, std::size_t sizeOfParameter, int requestedJobNumber) {
    if (!self) throw std::invalid_argument("null WTF::ParallelEnvironment");
    const int maxNumberOfCores = _ZN3WTF22numberOfProcessorCoresEv();
    if (!requestedJobNumber || requestedJobNumber > maxNumberOfCores) requestedJobNumber = maxNumberOfCores;
    const int maxNumberOfNewThreads = requestedJobNumber - 1;
    std::vector<Worker*> taken;
    {
        std::lock_guard poolLock(ThreadPoolLock());
        std::vector<Worker*>& pool = ThreadPool();
        for (int index = 0; index < maxNumberOfCores && taken.size() < static_cast<unsigned>(maxNumberOfNewThreads); ++index) {
            if (pool.size() < static_cast<unsigned>(index) + 1U) pool.push_back(new Worker);
            if (TryLockFor(*pool[index], self)) taken.push_back(pool[index]);
        }
    }
    const auto count = static_cast<unsigned>(taken.size());
    ParallelEnvironmentThread** buffer = nullptr;
    if (count) {
        buffer = static_cast<ParallelEnvironmentThread**>(_ZN3WTF10fastMallocEm(count * sizeof(ParallelEnvironmentThread*)));
        for (unsigned index = 0; index < count; ++index) {
            ++taken[index]->refCount;
            buffer[index] = taken[index];
        }
    }
    self->threadFunction = threadFunction;
    self->sizeOfParameter = sizeOfParameter;
    self->numberOfJobs = static_cast<int>(count) + 1;
    self->threads = {buffer, count, count};
}

}  // namespace

extern "C" {

void APS5_VABI _ZN3WTF19ParallelEnvironmentC1EPFvPvEmi(
    ParallelEnvironment* self, ParallelEnvironmentFunction threadFunction, std::size_t sizeOfParameter, int requestedJobNumber) {
    Construct(self, threadFunction, sizeOfParameter, requestedJobNumber);
}

void APS5_VABI _ZN3WTF19ParallelEnvironmentC2EPFvPvEmi(
    ParallelEnvironment* self, ParallelEnvironmentFunction threadFunction, std::size_t sizeOfParameter, int requestedJobNumber) {
    Construct(self, threadFunction, sizeOfParameter, requestedJobNumber);
}

void APS5_VABI _ZN3WTF19ParallelEnvironment7executeEPv(ParallelEnvironment* self, void* parameters) {
    if (!self) throw std::invalid_argument("null WTF::ParallelEnvironment");
    if (!self->threadFunction) throw std::invalid_argument("WTF::ParallelEnvironment::execute: null thread function");
    auto* currentParameter = static_cast<unsigned char*>(parameters);
    for (unsigned index = 0; index < self->threads.size; ++index) {
        Worker& worker = WorkerOf(self->threads.buffer[index]);
        {
            std::lock_guard lock(worker.lock);
            worker.threadFunction = self->threadFunction;
            worker.parameters = currentParameter;
            worker.running = true;
        }
        worker.condition.notify_one();
        currentParameter += self->sizeOfParameter;
    }
    self->threadFunction(currentParameter);
    for (unsigned index = 0; index < self->threads.size; ++index) {
        Worker& worker = WorkerOf(self->threads.buffer[index]);
        std::unique_lock lock(worker.lock);
        worker.condition.wait(lock, [&worker] { return !worker.running; });
    }
}

}
