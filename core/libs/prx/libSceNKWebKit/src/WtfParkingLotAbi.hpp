#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfTimeTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::WtfAbi {

template<typename Result, typename... Arguments>
struct ScopedLambda {
    Result (APS5_VABI* impl)(void* argument, Arguments... arguments);
    void* argument;
};

struct ParkResult {
    bool wasUnparked{false};
    std::intptr_t token{0};
};

struct UnparkResult {
    bool didUnparkThread{false};
    bool mayHaveMoreThreads{false};
    bool timeToBeFair{false};
};

using ValidationLambda = ScopedLambda<bool>;
using BeforeSleepLambda = ScopedLambda<void>;
using UnparkCallbackLambda = ScopedLambda<std::intptr_t, UnparkResult>;

struct Lock {
    std::uint8_t byte;
};

struct Condition {
    std::uint8_t hasWaiters;
};

struct WordLock {
    std::uintptr_t word;
};

struct ReadWriteLock {
    Lock lock;
    Condition condition;
    bool isWriteLocked;
    unsigned numReaders;
    unsigned numWaitingWriters;
};

struct BinarySemaphore {
    bool isSet;
    Lock lock;
    Condition condition;
};

static_assert(sizeof(ValidationLambda) == 16);
static_assert(offsetof(ValidationLambda, argument) == 8);
static_assert(sizeof(ParkResult) == 16);
static_assert(offsetof(ParkResult, token) == 8);
static_assert(std::is_trivially_copyable_v<ParkResult>);
static_assert(sizeof(UnparkResult) == 3);
static_assert(std::is_trivially_copyable_v<UnparkResult>);
static_assert(sizeof(Lock) == 1);
static_assert(sizeof(Condition) == 1);
static_assert(sizeof(WordLock) == 8);
static_assert(sizeof(ReadWriteLock) == 12);
static_assert(alignof(ReadWriteLock) == 4);
static_assert(offsetof(ReadWriteLock, condition) == 1);
static_assert(offsetof(ReadWriteLock, isWriteLocked) == 2);
static_assert(offsetof(ReadWriteLock, numReaders) == 4);
static_assert(offsetof(ReadWriteLock, numWaitingWriters) == 8);
static_assert(sizeof(BinarySemaphore) == 3);
static_assert(offsetof(BinarySemaphore, lock) == 1);
static_assert(offsetof(BinarySemaphore, condition) == 2);
static_assert(sizeof(WTF::TimeWithDynamicClockType) == 16);

}  // namespace NKWebKit::WtfAbi
