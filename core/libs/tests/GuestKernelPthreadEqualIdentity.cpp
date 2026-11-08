#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "assertion failed at line %d\n", line);
        std::abort();
    }
}
#define Check(x) Require((x), __LINE__)

struct WorkerContext {
    Pthread mainThread = nullptr;
    std::array<Pthread, 3> workerIds{};
    std::atomic<int> startedCount{0};
    std::atomic<bool> release{false};
    std::atomic<int> finishedCount{0};
};

static void* APS5_VABI WorkerEntry(void* argument) {
    auto* context = static_cast<WorkerContext*>(argument);
    const Pthread selfPosix = pthread_self_nid_postfix();
    const Pthread selfSce = scePthreadSelf();

    Check(selfPosix != nullptr);
    Check(selfSce != nullptr);
    Check(pthread_equal_nid_postfix(selfPosix, selfPosix) != 0);
    Check(scePthreadEqual(selfSce, selfSce) != 0);
    Check(pthread_equal_nid_postfix(selfPosix, selfSce) != 0);
    Check(scePthreadEqual(selfPosix, selfSce) != 0);
    Check(pthread_equal_nid_postfix(selfPosix, nullptr) == 0);
    Check(pthread_equal_nid_postfix(nullptr, selfPosix) == 0);
    Check(scePthreadEqual(selfPosix, nullptr) == 0);
    Check(scePthreadEqual(nullptr, selfPosix) == 0);
    Check(pthread_equal_nid_postfix(selfPosix, context->mainThread) == 0);
    Check(pthread_equal_nid_postfix(context->mainThread, selfPosix) == 0);
    Check(scePthreadEqual(selfPosix, context->mainThread) == 0);
    Check(scePthreadEqual(context->mainThread, selfPosix) == 0);

    const int index = context->startedCount.fetch_add(1);
    context->workerIds[static_cast<std::size_t>(index)] = selfPosix;

    while (!context->release.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    for (std::size_t i = 0; i < context->workerIds.size(); ++i) {
        const Pthread other = context->workerIds[i];
        Check(other != nullptr);
        if (static_cast<int>(i) == index) {
            Check(pthread_equal_nid_postfix(selfPosix, other) != 0);
            Check(scePthreadEqual(selfPosix, other) != 0);
        } else {
            Check(pthread_equal_nid_postfix(selfPosix, other) == 0);
            Check(scePthreadEqual(selfPosix, other) == 0);
        }
    }

    context->finishedCount.fetch_add(1);
    return nullptr;
}

int main() {
    const Pthread nullThread = nullptr;
    const Pthread zeroThread = 0;

    Check(pthread_equal_nid_postfix(nullptr, nullptr) != 0);
    Check(pthread_equal_nid_postfix(nullThread, nullThread) != 0);
    Check(pthread_equal_nid_postfix(nullThread, zeroThread) != 0);
    Check(pthread_equal_nid_postfix(zeroThread, nullThread) != 0);
    Check(pthread_equal_nid_postfix(nullptr, nullThread) != 0);
    Check(pthread_equal_nid_postfix(nullThread, nullptr) != 0);

    Check(scePthreadEqual(nullptr, nullptr) != 0);
    Check(scePthreadEqual(nullThread, nullThread) != 0);
    Check(scePthreadEqual(nullThread, zeroThread) != 0);
    Check(scePthreadEqual(zeroThread, nullThread) != 0);
    Check(scePthreadEqual(nullptr, nullThread) != 0);
    Check(scePthreadEqual(nullThread, nullptr) != 0);

    const Pthread mainPosix = pthread_self_nid_postfix();
    const Pthread mainSce = scePthreadSelf();

    Check(mainPosix != nullptr);
    Check(mainSce != nullptr);
    Check(pthread_equal_nid_postfix(mainPosix, mainPosix) != 0);
    Check(pthread_equal_nid_postfix(mainSce, mainSce) != 0);
    Check(pthread_equal_nid_postfix(mainPosix, mainSce) != 0);
    Check(pthread_equal_nid_postfix(mainSce, mainPosix) != 0);

    Check(scePthreadEqual(mainPosix, mainPosix) != 0);
    Check(scePthreadEqual(mainSce, mainSce) != 0);
    Check(scePthreadEqual(mainPosix, mainSce) != 0);
    Check(scePthreadEqual(mainSce, mainPosix) != 0);

    Check(pthread_equal_nid_postfix(mainPosix, nullptr) == 0);
    Check(pthread_equal_nid_postfix(nullptr, mainPosix) == 0);
    Check(pthread_equal_nid_postfix(mainPosix, nullThread) == 0);
    Check(pthread_equal_nid_postfix(nullThread, mainPosix) == 0);

    Check(scePthreadEqual(mainPosix, nullptr) == 0);
    Check(scePthreadEqual(nullptr, mainPosix) == 0);
    Check(scePthreadEqual(mainPosix, nullThread) == 0);
    Check(scePthreadEqual(nullThread, mainPosix) == 0);

    const Pthread fake1 = reinterpret_cast<Pthread>(std::uintptr_t{0x1000});
    const Pthread fake2 = reinterpret_cast<Pthread>(std::uintptr_t{0x2000});
    const Pthread fake1Alias = reinterpret_cast<Pthread>(std::uintptr_t{0x1000});

    Check(pthread_equal_nid_postfix(fake1, fake1) != 0);
    Check(pthread_equal_nid_postfix(fake1, fake1Alias) != 0);
    Check(pthread_equal_nid_postfix(fake1, fake2) == 0);
    Check(pthread_equal_nid_postfix(fake2, fake1) == 0);
    Check(pthread_equal_nid_postfix(fake1, nullptr) == 0);
    Check(pthread_equal_nid_postfix(nullptr, fake1) == 0);
    Check(pthread_equal_nid_postfix(fake1, mainPosix) == 0);
    Check(pthread_equal_nid_postfix(mainPosix, fake1) == 0);

    Check(scePthreadEqual(fake1, fake1) != 0);
    Check(scePthreadEqual(fake1, fake1Alias) != 0);
    Check(scePthreadEqual(fake1, fake2) == 0);
    Check(scePthreadEqual(fake2, fake1) == 0);
    Check(scePthreadEqual(fake1, nullptr) == 0);
    Check(scePthreadEqual(nullptr, fake1) == 0);
    Check(scePthreadEqual(fake1, mainPosix) == 0);
    Check(scePthreadEqual(mainPosix, fake1) == 0);

    WorkerContext context{};
    context.mainThread = mainPosix;

    std::array<Pthread, 3> createdThreads{};
    for (std::size_t i = 0; i < createdThreads.size(); ++i) {
        if (i % 2 == 0) {
            Check(scePthreadCreate(&createdThreads[i], nullptr, WorkerEntry, &context, "worker") == 0);
        } else {
            Check(pthread_create_name_np_nid_postfix(&createdThreads[i], nullptr, WorkerEntry, &context, "worker") == 0);
        }
        Check(createdThreads[i] != nullptr);
    }

    while (context.startedCount.load(std::memory_order_acquire) < 3) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    for (std::size_t i = 0; i < createdThreads.size(); ++i) {
        Check(pthread_equal_nid_postfix(createdThreads[i], createdThreads[i]) != 0);
        Check(scePthreadEqual(createdThreads[i], createdThreads[i]) != 0);
        Check(pthread_equal_nid_postfix(createdThreads[i], context.workerIds[i]) != 0);
        Check(scePthreadEqual(createdThreads[i], context.workerIds[i]) != 0);
        Check(pthread_equal_nid_postfix(createdThreads[i], mainPosix) == 0);
        Check(scePthreadEqual(createdThreads[i], mainPosix) == 0);
        Check(pthread_equal_nid_postfix(createdThreads[i], nullptr) == 0);
        Check(scePthreadEqual(createdThreads[i], nullptr) == 0);

        for (std::size_t j = i + 1; j < createdThreads.size(); ++j) {
            Check(pthread_equal_nid_postfix(createdThreads[i], createdThreads[j]) == 0);
            Check(pthread_equal_nid_postfix(createdThreads[j], createdThreads[i]) == 0);
            Check(scePthreadEqual(createdThreads[i], createdThreads[j]) == 0);
            Check(scePthreadEqual(createdThreads[j], createdThreads[i]) == 0);
        }
    }

    const std::array<Pthread, 7> allHandles = {
        nullptr,
        mainPosix,
        createdThreads[0],
        createdThreads[1],
        createdThreads[2],
        fake1,
        fake2
    };

    for (std::size_t i = 0; i < allHandles.size(); ++i) {
        for (std::size_t j = 0; j < allHandles.size(); ++j) {
            const bool identical = (allHandles[i] == allHandles[j]);
            const int posixEqual = pthread_equal_nid_postfix(allHandles[i], allHandles[j]);
            const int sceEqual = scePthreadEqual(allHandles[i], allHandles[j]);

            Check((posixEqual != 0) == identical);
            Check((sceEqual != 0) == identical);
            Check(posixEqual == pthread_equal_nid_postfix(allHandles[j], allHandles[i]));
            Check(sceEqual == scePthreadEqual(allHandles[j], allHandles[i]));
            Check((posixEqual != 0) == (sceEqual != 0));
        }
    }

    context.release.store(true, std::memory_order_release);

    for (std::size_t i = 0; i < createdThreads.size(); ++i) {
        void* returnValue = nullptr;
        if (i % 2 == 0) {
            Check(scePthreadJoin(createdThreads[i], &returnValue) == 0);
        } else {
            Check(pthread_join_nid_postfix(createdThreads[i], &returnValue) == 0);
        }
        Check(returnValue == nullptr);
    }

    Check(context.finishedCount.load(std::memory_order_acquire) == 3);

    return 0;
}
