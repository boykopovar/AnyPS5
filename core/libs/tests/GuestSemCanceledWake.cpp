#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <thread>

namespace {

void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

template<class TPredicate>
bool Until(TPredicate predicate, int milliseconds = 3000) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= end) return false;
        std::this_thread::yield();
    }
    return true;
}

struct Context {
    PthreadSem* semaphore;
    bool timed;
    bool disabled;
    std::atomic<bool> returned{false};
};

void* APS5_VABI Waiter(void* argument) {
    auto& context = *static_cast<Context*>(argument);
    if (context.disabled) Require(scePthreadSetcancelstate(GUEST_CANCEL_DISABLE, nullptr) == 0, "disable cancellation");
    const auto result = context.timed ? scePthreadSemTimedwait(context.semaphore, 10000000) : scePthreadSemWait(context.semaphore);
    Require(result == 0, "semaphore wait failed");
    context.returned = true;
    return &context;
}

Pthread Start(Context& context) {
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Waiter, &context, nullptr) == 0, "create waiter");
    Require(Until([&] {
        std::lock_guard semaphoreLock((*context.semaphore)->_mutex);
        std::lock_guard cancellationLock(thread->cancelLock);
        return thread->cancelWait == &(*context.semaphore)->_cv;
    }), "waiter did not enter semaphore wait");
    return thread;
}

bool Awakened(Pthread thread) {
    std::lock_guard lock(thread->cancelLock);
    return thread->cancelWait == nullptr;
}

void Join(Pthread thread, void* expected) {
    void* result = nullptr;
    Require(scePthreadJoin(thread, &result) == 0 && result == expected, "unexpected waiter result");
}

int Value(PthreadSem* semaphore) {
    int value = -1;
    Require(scePthreadSemGetvalue(semaphore, &value) == 0, "get semaphore value");
    return value;
}

enum class Outcome { Passed, Failed, Retry };

Outcome PostThenCancel(bool timed, bool disabled) {
    PthreadSem semaphore = nullptr;
    Require(scePthreadSemInit(&semaphore, 0, 0, nullptr) == 0, "initialize semaphore");
    Context contexts[2]{{&semaphore, timed, disabled}, {&semaphore, timed, disabled}};
    const auto first = Start(contexts[0]);
    const auto second = Start(contexts[1]);
    std::unique_lock firstGate(first->cancelLock, std::defer_lock);
    std::unique_lock secondGate(second->cancelLock, std::defer_lock);
    Require(Until([&] {
        firstGate.lock();
        secondGate.lock();
        if (first->cancelWait == &semaphore->_cv && second->cancelWait == &semaphore->_cv) return true;
        secondGate.unlock();
        firstGate.unlock();
        return false;
    }), "waiters did not remain registered before post");
    Require(scePthreadSemPost(&semaphore) == 0, "post semaphore");
    std::unique_lock semaphoreGate(semaphore->_mutex);
    firstGate.unlock();
    secondGate.unlock();
    Require(Until([&] { return Awakened(first) || Awakened(second); }), "no waiter left its condition wait");
    const bool firstAwoke = Awakened(first);
    const auto canceledThread = firstAwoke ? first : second;
    const auto survivingThread = firstAwoke ? second : first;
    auto& canceled = contexts[firstAwoke ? 0 : 1];
    auto& survivor = contexts[firstAwoke ? 1 : 0];
    if (Awakened(survivingThread)) {
        semaphoreGate.unlock();
        Require(scePthreadSemPost(&semaphore) == 0, "release a spurious wake attempt");
        Join(first, &contexts[0]);
        Join(second, &contexts[1]);
        Require(Value(&semaphore) == 0, "spurious wake changed token count");
        Require(scePthreadSemDestroy(&semaphore) == 0, "destroy spurious wake attempt");
        return Outcome::Retry;
    }
    Require(scePthreadCancel(canceledThread) == 0, "cancel awakened waiter");
    semaphoreGate.unlock();
    Join(canceledThread, disabled ? static_cast<void*>(&canceled) : GUEST_CANCELED);
    Require(canceled.returned.load() == disabled, "cancellation consumed a semaphore token");
    const bool completed = disabled ? survivor.returned.load() : Until([&] { return survivor.returned.load(); }, 250);
    const int value = Value(&semaphore);
    std::printf("timed=%d disabled=%d survivor_returned=%d unclaimed_tokens=%d\n", timed, disabled, completed, value);
    if (!completed) Require(scePthreadSemPost(&semaphore) == 0, "release surviving waiter");
    Join(survivingThread, &survivor);
    Require(scePthreadSemDestroy(&semaphore) == 0, "destroy semaphore");
    return (disabled ? !completed && value == 0 : completed && value == 0) ? Outcome::Passed : Outcome::Failed;
}

void CancelWithoutPost(bool timed) {
    PthreadSem semaphore = nullptr;
    Require(scePthreadSemInit(&semaphore, 0, 0, nullptr) == 0, "initialize cancellation-only semaphore");
    Context canceled{&semaphore, timed, false}, survivor{&semaphore, timed, false};
    const auto first = Start(canceled);
    const auto second = Start(survivor);
    Require(scePthreadCancel(first) == 0, "cancel without post");
    Join(first, GUEST_CANCELED);
    Require(!canceled.returned.load() && !survivor.returned.load() && Value(&semaphore) == 0, "cancellation created a semaphore token");
    Require(scePthreadSemPost(&semaphore) == 0, "release cancellation-only survivor");
    Join(second, &survivor);
    Require(Value(&semaphore) == 0, "cancellation-only survivor lost a token");
    Require(scePthreadSemDestroy(&semaphore) == 0, "destroy cancellation-only semaphore");
}

}

int main() {
    try {
        bool correct = true;
        for (bool timed : {false, true}) {
            CancelWithoutPost(timed);
            for (bool disabled : {false, true}) {
                Outcome outcome = Outcome::Retry;
                for (int attempt = 0; attempt < 8 && outcome == Outcome::Retry; ++attempt) outcome = PostThenCancel(timed, disabled);
                Require(outcome != Outcome::Retry, "could not isolate a single awakened waiter");
                correct = outcome == Outcome::Passed && correct;
            }
        }
        Require(correct, "a canceled awakened waiter stranded an available semaphore token");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
