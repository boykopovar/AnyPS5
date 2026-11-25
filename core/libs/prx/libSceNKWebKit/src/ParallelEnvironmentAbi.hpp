#pragma once

#include "SceTypes.hpp"

#include <cstddef>

namespace NKWebKit::WtfAbi {

struct ParallelEnvironmentThread {
    unsigned refCount;
};

struct ParallelEnvironmentThreadVector {
    ParallelEnvironmentThread** buffer;
    unsigned capacity;
    unsigned size;
};

using ParallelEnvironmentFunction = void (APS5_VABI*)(void* parameter);

struct ParallelEnvironment {
    ParallelEnvironmentFunction threadFunction;
    std::size_t sizeOfParameter;
    int numberOfJobs;
    ParallelEnvironmentThreadVector threads;
};

static_assert(sizeof(ParallelEnvironmentThreadVector) == 16);
static_assert(offsetof(ParallelEnvironmentThreadVector, capacity) == 8);
static_assert(offsetof(ParallelEnvironmentThreadVector, size) == 12);
static_assert(sizeof(ParallelEnvironment) == 40);
static_assert(offsetof(ParallelEnvironment, threadFunction) == 0);
static_assert(offsetof(ParallelEnvironment, sizeOfParameter) == 8);
static_assert(offsetof(ParallelEnvironment, numberOfJobs) == 16);
static_assert(offsetof(ParallelEnvironment, threads) == 24);
static_assert(offsetof(ParallelEnvironmentThread, refCount) == 0);

}  // namespace NKWebKit::WtfAbi
