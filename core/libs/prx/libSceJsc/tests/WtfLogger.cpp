#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <type_traits>

namespace {

using Jsc::WtfAbi::LoggerObserverVector;
using ObserversFunction = LoggerObserverVector* (APS5_VABI*)();

static_assert(std::is_same_v<decltype(&_ZN3WTF6Logger9observersEv), ObserversFunction>);

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF Logger check failed at line %d\n", line);
        std::abort();
    }
}

LoggerObserverVector* Observers() {
    return _ZN3WTF6Logger9observersEv();
}

void TestLayout() {
    Check(sizeof(LoggerObserverVector) == 16, __LINE__);
    Check(offsetof(LoggerObserverVector, buffer) == 0, __LINE__);
    Check(offsetof(LoggerObserverVector, capacity) == 8, __LINE__);
    Check(offsetof(LoggerObserverVector, size) == 12, __LINE__);
}

void TestStartsEmpty() {
    LoggerObserverVector* observers = Observers();
    Check(observers != nullptr, __LINE__);
    Check(reinterpret_cast<std::uintptr_t>(observers) % alignof(LoggerObserverVector) == 0, __LINE__);
    Check(observers->buffer == nullptr, __LINE__);
    Check(observers->capacity == 0, __LINE__);
    Check(observers->size == 0, __LINE__);
}

void TestSameVectorOnEveryCall() {
    LoggerObserverVector* first = Observers();
    Check(Observers() == first, __LINE__);
    LoggerObserverVector* fromThread = nullptr;
    std::thread thread([&] { fromThread = Observers(); });
    thread.join();
    Check(fromThread == first, __LINE__);
}

void TestKeepsRegisteredObservers() {
    int firstObserver = 1;
    int secondObserver = 2;
    void* storage[4] = {};
    LoggerObserverVector* observers = Observers();
    observers->buffer = storage;
    observers->capacity = 4;
    observers->buffer[observers->size++] = &firstObserver;
    std::thread thread([&] {
        LoggerObserverVector* shared = Observers();
        shared->buffer[shared->size++] = &secondObserver;
    });
    thread.join();
    LoggerObserverVector* again = Observers();
    Check(again->buffer == storage, __LINE__);
    Check(again->capacity == 4, __LINE__);
    Check(again->size == 2, __LINE__);
    Check(again->buffer[0] == &firstObserver, __LINE__);
    Check(again->buffer[1] == &secondObserver, __LINE__);
    *again = {};
    Check(Observers()->buffer == nullptr && Observers()->size == 0, __LINE__);
}

} // namespace

int main() {
    TestLayout();
    TestStartsEmpty();
    TestSameVectorOnEveryCall();
    TestKeepsRegisteredObservers();
    return 0;
}
