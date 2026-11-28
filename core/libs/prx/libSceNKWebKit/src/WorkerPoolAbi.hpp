#pragma once

#include "SceTypes.hpp"

#include <atomic>
#include <cstddef>

namespace NKWebKit::WtfAbi {

struct WorkerPoolState;

struct WorkerPool {
    std::atomic<unsigned> refCount;
    WorkerPoolState* state;
    unsigned char unused[80];
    const char* name;
};

static_assert(sizeof(WorkerPool) == 104);
static_assert(offsetof(WorkerPool, refCount) == 0);
static_assert(offsetof(WorkerPool, state) == 8);
static_assert(offsetof(WorkerPool, unused) == 16);
static_assert(offsetof(WorkerPool, name) == 96);

}  // namespace NKWebKit::WtfAbi
