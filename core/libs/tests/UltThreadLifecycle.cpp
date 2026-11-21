#include "prx/libSceUlt/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Cancel.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <semaphore>
#include <stdexcept>
#include <thread>
#include "../prx/libSceUlt/Export.cpp"

namespace {

void Require(bool value, int line) {
    if (value) return;
    std::fprintf(stderr, "ULT lifecycle check failed at line %d\n", line);
    std::abort();
}

#define CHECK(value) Require((value), __LINE__)

struct alignas(8) Runtime { std::byte bytes[4096]{}; };
struct alignas(8) Thread { std::byte bytes[512]{}; };

struct HostThread {
    std::thread thread;
    void* result = nullptr;
    std::uint64_t generation = 0;
};

std::atomic<bool> entryFinished{false};
std::atomic<int> joinCalls{0};
std::atomic<int> hostThreads{0};
std::binary_semaphore joining{0};
std::binary_semaphore releaseJoin{0};
bool gateJoin = false;
int createFailure = 0;
int joinFailure = 0;
int sentinelExecutions = 0;

void APS5_VABI Sentinel(void*) {
    ++sentinelExecutions;
}

#ifdef ULT_LIFECYCLE_INTERPOSE
thread_local int actor = 0;
std::atomic<bool> pauseSelection{false};
std::atomic<bool> failCleanupSetup{false};
std::atomic<std::uint64_t> originalGeneration{0};
std::binary_semaphore selected{0};
std::binary_semaphore releaseSelection{0};
std::binary_semaphore creating{0};
std::binary_semaphore releaseCreate{0};
std::binary_semaphore registryAttempt{0};
std::binary_semaphore releaseRegistryAttempt{0};
std::binary_semaphore replacementStarted{0};
std::binary_semaphore releaseReplacement{0};
bool gateCreate = false;
#endif

void Await(std::binary_semaphore& gate) {
    CHECK(gate.try_acquire_for(std::chrono::seconds(2)));
}

std::int32_t APS5_VABI Entry(std::uint64_t argument) {
    void* self = nullptr;
    CHECK(sceUltUlthreadGetSelf(&self) == ULT_OK && self != nullptr);
    CHECK(sceUltUlthreadYield() == ULT_OK);
    return static_cast<std::int32_t>(argument);
}

void Create(Runtime& runtime, Thread& thread) {
    CHECK(sceUltUlthreadRuntimeCreate(&runtime, "runtime", 1, 1, nullptr, nullptr, 0) == ULT_OK);
    CHECK(sceUltUlthreadCreate(&thread, "thread", Entry, 73, nullptr, 0, &runtime, nullptr, 0) == ULT_OK);
    entryFinished.wait(false);
}

void Finish(Runtime& runtime, Thread& thread) {
    std::int32_t status = -1;
    CHECK(sceUltUlthreadJoin(&thread, &status) == ULT_OK && status == 73);
    CHECK(hostThreads.load() == 0);
    CHECK(sceUltUlthreadJoin(&thread, &status) == ULT_ERROR_STATE && status == 73);
    CHECK(sceUltUlthreadRuntimeDestroy(&runtime) == ULT_OK);
    CHECK(sceUltFinalize() == ULT_OK);
}

void CreateFailure(int failure) {
    Runtime runtime;
    Thread thread;
    CHECK(sceUltUlthreadRuntimeCreate(&runtime, "runtime", 1, 1, nullptr, nullptr, 0) == ULT_OK);
    createFailure = failure;
    bool threw = false;
    int result = ULT_OK;
    try {
        result = sceUltUlthreadCreate(&thread, "thread", Entry, 73, nullptr, 0, &runtime, nullptr, 0);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw == (failure != 1));
    if (failure == 1) CHECK(result == ULT_ERROR_AGAIN);
    if (failure == 3) {
        CHECK(sceUltFinalize() == ULT_ERROR_BUSY);
        CHECK(sceUltUlthreadRuntimeDestroy(&runtime) == ULT_ERROR_STATE);
        Finish(runtime, thread);
        return;
    }
    CHECK(hostThreads.load() == 0 && !entryFinished.load());
    CHECK(sceUltUlthreadTryJoin(&thread, nullptr) == ULT_ERROR_STATE);
    CHECK(sceUltUlthreadCreate(&thread, "retry", Entry, 73, nullptr, 0, &runtime, nullptr, 0) == ULT_OK);
    Finish(runtime, thread);
}

void Ownership(bool nonblocking) {
    Runtime runtime;
    Thread thread;
    Create(runtime, thread);
    gateJoin = true;
    std::int32_t firstStatus = -1;
    int firstResult = ULT_ERROR_STATE;
    std::thread first([&] {
        firstResult = nonblocking ? sceUltUlthreadTryJoin(&thread, &firstStatus) : sceUltUlthreadJoin(&thread, &firstStatus);
    });
    Await(joining);
    std::int32_t secondStatus = -2;
    CHECK(sceUltUlthreadJoin(&thread, &secondStatus) == ULT_ERROR_STATE && secondStatus == -2);
    CHECK(sceUltUlthreadTryJoin(&thread, &secondStatus) == ULT_ERROR_STATE && secondStatus == -2);
    CHECK(joinCalls.load() == 1);
    CHECK(sceUltUlthreadCreate(&thread, "replacement", Entry, 74, nullptr, 0, &runtime, nullptr, 0) == ULT_ERROR_STATE);
    CHECK(sceUltUlthreadRuntimeDestroy(&runtime) == ULT_ERROR_STATE);
    CHECK(sceUltFinalize() == ULT_ERROR_BUSY);
    releaseJoin.release();
    first.join();
    CHECK(firstResult == ULT_OK && firstStatus == 73 && hostThreads.load() == 0);
    CHECK(sceUltUlthreadTryJoin(&thread, &secondStatus) == ULT_ERROR_STATE && secondStatus == -2);
    gateJoin = false;
    entryFinished.store(false);
    CHECK(sceUltUlthreadCreate(&thread, "replacement", Entry, 73, nullptr, 0, &runtime, nullptr, 0) == ULT_OK);
    Finish(runtime, thread);
}

void JoinFailure(int failure) {
    Runtime runtime;
    Thread thread;
    Create(runtime, thread);
    void* const before = ThreadCancel::CleanupMark();
    if (failure == 3) pthread_cleanup_push_nid_postfix(Sentinel, nullptr);
    joinFailure = failure;
    std::int32_t status = -1;
    bool threw = false;
    int result = ULT_OK;
    try {
        result = sceUltUlthreadJoin(&thread, &status);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw == (failure >= 2));
    if (!threw) CHECK(result == ULT_ERROR_STATE);
    CHECK(status == -1 && hostThreads.load() == 1);
    if (failure == 3) {
        CHECK(sentinelExecutions == 1 && ThreadCancel::CleanupMark() == before);
        std::thread replacement([&] { Finish(runtime, thread); });
        replacement.join();
    } else Finish(runtime, thread);
}

#ifdef ULT_LIFECYCLE_INTERPOSE
void Publication() {
    Runtime runtime;
    Thread thread;
    CHECK(sceUltUlthreadRuntimeCreate(&runtime, "runtime", 1, 1, nullptr, nullptr, 0) == ULT_OK);
    gateCreate = true;
    std::thread creator([&] {
        CHECK(sceUltUlthreadCreate(&thread, "thread", Entry, 73, nullptr, 0, &runtime, nullptr, 0) == ULT_OK);
    });
    Await(creating);
    std::int32_t status = -1;
    std::thread joiner([&] {
        actor = 2;
        CHECK(sceUltUlthreadJoin(&thread, &status) == ULT_OK);
    });
    Await(registryAttempt);
    const bool registryAvailable = gMutex.try_lock();
    if (registryAvailable) gMutex.unlock();
    CHECK(!registryAvailable && joinCalls.load() == 0);
    releaseRegistryAttempt.release();
    releaseCreate.release();
    creator.join();
    joiner.join();
    CHECK(status == 73 && hostThreads.load() == 0);
    CHECK(sceUltUlthreadRuntimeDestroy(&runtime) == ULT_OK);
    CHECK(sceUltFinalize() == ULT_OK);
}

std::int32_t APS5_VABI Replacement(std::uint64_t argument) {
    replacementStarted.release();
    Await(releaseReplacement);
    return static_cast<std::int32_t>(argument);
}

void Generation() {
    Runtime runtime;
    Thread thread;
    Create(runtime, thread);
    pauseSelection.store(true);
    std::int32_t originalStatus = -1;
    int originalResult = ULT_ERROR_STATE;
    std::thread original([&] {
        actor = 1;
        originalResult = sceUltUlthreadTryJoin(&thread, &originalStatus);
    });
    Await(selected);
    std::int32_t contenderStatus = -2;
    const int contender = sceUltUlthreadJoin(&thread, &contenderStatus);
    if (contender == ULT_OK) {
        CHECK(contenderStatus == 73);
        CHECK(sceUltUlthreadCreate(&thread, "replacement", Replacement, 92, nullptr, 0, &runtime, nullptr, 0) == ULT_OK);
        Await(replacementStarted);
    } else {
        CHECK(contender == ULT_ERROR_STATE && contenderStatus == -2);
        CHECK(sceUltUlthreadCreate(&thread, "replacement", Replacement, 92, nullptr, 0, &runtime, nullptr, 0) == ULT_ERROR_STATE);
    }
    releaseSelection.release();
    original.join();
    CHECK(originalGeneration.load() == 73 && originalResult == ULT_OK && originalStatus == 73);
    CHECK(contender == ULT_ERROR_STATE && hostThreads.load() == 0);
    CHECK(sceUltUlthreadCreate(&thread, "replacement", Replacement, 92, nullptr, 0, &runtime, nullptr, 0) == ULT_OK);
    Await(replacementStarted);
    CHECK(sceUltUlthreadTryJoin(&thread, &contenderStatus) == ULT_ERROR_AGAIN && contenderStatus == -2);
    releaseReplacement.release();
    CHECK(sceUltUlthreadJoin(&thread, &contenderStatus) == ULT_OK && contenderStatus == 92);
    CHECK(sceUltUlthreadRuntimeDestroy(&runtime) == ULT_OK);
    CHECK(sceUltFinalize() == ULT_OK);
}

void CleanupSetup() {
    Runtime runtime;
    Thread thread;
    Create(runtime, thread);
    failCleanupSetup.store(true);
    std::int32_t status = -1;
    bool threw = false;
    try {
        sceUltUlthreadJoin(&thread, &status);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw && status == -1 && joinCalls.load() == 0);
    std::thread replacement([&] { Finish(runtime, thread); });
    replacement.join();
}
#endif

}

extern "C" {

#ifdef ULT_LIFECYCLE_INTERPOSE
int __real_pthread_mutex_lock(pthread_mutex_t* mutex);
int __real_pthread_mutex_unlock(pthread_mutex_t* mutex);
void APS5_VABI __real___pthread_cleanup_push_imp_nid_postfix(void (APS5_VABI* routine)(void*), void* argument, void* info);

int __wrap_pthread_mutex_lock(pthread_mutex_t* mutex) {
    if (actor == 2 && mutex == gMutex.native_handle()) {
        actor = 0;
        registryAttempt.release();
        Await(releaseRegistryAttempt);
    }
    return __real_pthread_mutex_lock(mutex);
}

int __wrap_pthread_mutex_unlock(pthread_mutex_t* mutex) {
    const int result = __real_pthread_mutex_unlock(mutex);
    if (actor == 1 && mutex == gMutex.native_handle() && pauseSelection.exchange(false)) {
        selected.release();
        Await(releaseSelection);
    }
    return result;
}

void APS5_VABI __wrap___pthread_cleanup_push_imp_nid_postfix(void (APS5_VABI* routine)(void*), void* argument, void* info) {
    if (failCleanupSetup.exchange(false)) throw std::runtime_error("Injected cleanup setup failure");
    __real___pthread_cleanup_push_imp_nid_postfix(routine, argument, info);
}
#endif

int APS5_VABI scePthreadCreate(Pthread* value, const PthreadAttr*, PthreadEntry entry, void* argument, const char*) {
    const int failure = createFailure;
    createFailure = 0;
    if (failure == 1) return 11;
    if (failure == 2) throw std::runtime_error("Injected prepublication creation failure");
#ifdef ULT_LIFECYCLE_INTERPOSE
    if (gateCreate) {
        gateCreate = false;
        creating.release();
        Await(releaseCreate);
    }
#endif
    auto state = std::make_unique<HostThread>();
    state->generation = static_cast<UltUlthreadState*>(argument)->_arg;
    auto* raw = state.get();
    state->thread = std::thread([=] {
        raw->result = entry(argument);
        entryFinished.store(true);
        entryFinished.notify_all();
    });
    ++hostThreads;
    *value = reinterpret_cast<Pthread>(state.release());
    if (failure == 3) throw std::runtime_error("Injected postpublication creation failure");
    return 0;
}

int APS5_VABI scePthreadJoin(Pthread value, void** result) {
    const int call = joinCalls.fetch_add(1);
#ifdef ULT_LIFECYCLE_INTERPOSE
    if (actor == 1) {
        CHECK(value != nullptr);
        const auto generation = reinterpret_cast<HostThread*>(value)->generation;
        originalGeneration.store(generation);
        if (generation != 73) return 22;
    }
#endif
    if (gateJoin) {
        if (call != 0) return 22;
        joining.release();
        Await(releaseJoin);
    }
    const int failure = joinFailure;
    joinFailure = 0;
    if (failure == 1) return 22;
    if (failure == 2) throw std::runtime_error("Injected precommit join failure");
    if (failure == 3) {
        __pthread_cleanup_pop_imp_nid_postfix(1);
        __pthread_cleanup_pop_imp_nid_postfix(1);
        throw std::runtime_error("Injected failure after cancellation cleanup");
    }
    CHECK(value != nullptr);
    std::unique_ptr<HostThread> state(reinterpret_cast<HostThread*>(value));
    state->thread.join();
    if (result != nullptr) *result = state->result;
    --hostThreads;
    return 0;
}

}

int main(int argc, char** argv) {
    CHECK(argc == 2);
    if (std::strcmp(argv[1], "create-error") == 0) CreateFailure(1);
    else if (std::strcmp(argv[1], "create-exception") == 0) CreateFailure(2);
    else if (std::strcmp(argv[1], "published-exception") == 0) CreateFailure(3);
    else if (std::strcmp(argv[1], "join-owner") == 0) Ownership(false);
    else if (std::strcmp(argv[1], "tryjoin-owner") == 0) Ownership(true);
    else if (std::strcmp(argv[1], "join-error") == 0) JoinFailure(1);
    else if (std::strcmp(argv[1], "join-exception") == 0) JoinFailure(2);
    else if (std::strcmp(argv[1], "join-cleanup-exception") == 0) JoinFailure(3);
#ifdef ULT_LIFECYCLE_INTERPOSE
    else if (std::strcmp(argv[1], "publication") == 0) Publication();
    else if (std::strcmp(argv[1], "generation") == 0) Generation();
    else if (std::strcmp(argv[1], "cleanup-setup") == 0) CleanupSetup();
#endif
    else CHECK(false);
}
