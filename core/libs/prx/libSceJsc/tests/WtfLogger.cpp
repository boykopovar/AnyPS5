#include "prx/libSceJsc/libSceJsc.hpp"
#include "Testing.hpp"

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

LoggerObserverVector* Observers() {
    return _ZN3WTF6Logger9observersEv();
}

void TestLayout() {
    Require(sizeof(LoggerObserverVector) == 16);
    Require(offsetof(LoggerObserverVector, buffer) == 0);
    Require(offsetof(LoggerObserverVector, capacity) == 8);
    Require(offsetof(LoggerObserverVector, size) == 12);
}

void TestStartsEmpty() {
    LoggerObserverVector* observers = Observers();
    Require(observers != nullptr);
    Require(reinterpret_cast<std::uintptr_t>(observers) % alignof(LoggerObserverVector) == 0);
    Require(observers->buffer == nullptr);
    Require(observers->capacity == 0);
    Require(observers->size == 0);
}

void TestSameVectorOnEveryCall() {
    LoggerObserverVector* first = Observers();
    Require(Observers() == first);
    LoggerObserverVector* fromThread = nullptr;
    std::thread thread([&] { fromThread = Observers(); });
    thread.join();
    Require(fromThread == first);
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
    Require(again->buffer == storage);
    Require(again->capacity == 4);
    Require(again->size == 2);
    Require(again->buffer[0] == &firstObserver);
    Require(again->buffer[1] == &secondObserver);
    *again = {};
    Require(Observers()->buffer == nullptr && Observers()->size == 0);
}

} // namespace

int main() {
    TestLayout();
    TestStartsEmpty();
    TestSameVectorOnEveryCall();
    TestKeepsRegisteredObservers();
    return 0;
}
