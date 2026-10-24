#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <cstdlib>
#include <thread>

static constexpr int SCE_OK = 0;

static void Require(bool value) { if (!value) std::abort(); }

static std::atomic<int> deletedCalls{0}, reusedCalls{0}, liveCalls{0};
static std::atomic<void*> liveValue{nullptr};
static std::atomic<int> stage{0};
static PthreadKey sharedKey = -1;
static int first = 1, second = 2, third = 3;

static void APS5_VABI Deleted(void*) { ++deletedCalls; }
static void APS5_VABI Reused(void*) { ++reusedCalls; }
static void APS5_VABI Live(void* value) { ++liveCalls; liveValue = value; }

static pthread_key_destructor_func_t Destructor(void (APS5_VABI* function)(void*)) {
    return reinterpret_cast<pthread_key_destructor_func_t>(function);
}

static void WaitFor(int value) {
    while (stage.load() != value) std::this_thread::yield();
}

static void* APS5_VABI ReuseBeforeExit(void*) {
    PthreadKey live = -1;
    Require(scePthreadKeyCreate(&live, Destructor(Live)) == SCE_OK);
    Require(scePthreadSetspecific(live, &third) == SCE_OK);
    PthreadKey deleted = -1;
    Require(scePthreadKeyCreate(&deleted, Destructor(Deleted)) == SCE_OK);
    Require(scePthreadSetspecific(deleted, &first) == SCE_OK);
    Require(scePthreadKeyDelete(deleted) == SCE_OK);
    PthreadKey reused = -1;
    Require(scePthreadKeyCreate(&reused, Destructor(Reused)) == SCE_OK);
    Require(reused == deleted);
    sharedKey = live;
    return nullptr;
}

static void* APS5_VABI HoldAcrossReuse(void*) {
    Require(scePthreadSetspecific(sharedKey, &first) == SCE_OK);
    Require(scePthreadGetspecific(sharedKey) == &first);
    stage = 1;
    WaitFor(2);
    Require(scePthreadGetspecific(sharedKey) == nullptr);
    Require(scePthreadSetspecific(sharedKey, &second) == SCE_OK);
    Require(scePthreadGetspecific(sharedKey) == &second);
    return nullptr;
}

int main() {
    PthreadKey posixKey = 0;
    Require(pthread_key_create_nid_postfix(&posixKey, nullptr) == 0 && posixKey != 0);
    Require(scePthreadSetspecific(0, &first) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadGetspecific(0) == nullptr);
    Require(scePthreadKeyDelete(0) == SCE_KERNEL_ERROR_EINVAL);
    Require(pthread_key_delete_nid_postfix(posixKey) == 0);

    PthreadKey key = -1;
    Require(scePthreadKeyCreate(&key, nullptr) == SCE_OK);
    Require(key != 0);
    Require(scePthreadGetspecific(key) == nullptr);
    Require(scePthreadSetspecific(key, &first) == SCE_OK);
    Require(scePthreadGetspecific(key) == &first);
    Require(scePthreadKeyDelete(key) == SCE_OK);
    Require(scePthreadGetspecific(key) == nullptr);
    Require(scePthreadSetspecific(key, &second) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadKeyDelete(key) == SCE_KERNEL_ERROR_EINVAL);

    PthreadKey reused = -1;
    Require(scePthreadKeyCreate(&reused, nullptr) == SCE_OK);
    Require(reused == key);
    Require(scePthreadGetspecific(reused) == nullptr);
    Require(scePthreadSetspecific(reused, &second) == SCE_OK);
    Require(scePthreadGetspecific(reused) == &second);
    Require(scePthreadKeyDelete(reused) == SCE_OK);

    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, ReuseBeforeExit, nullptr, nullptr) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(liveCalls == 1 && liveValue == &third);
    Require(deletedCalls == 0 && reusedCalls == 0);
    Require(scePthreadKeyDelete(sharedKey) == SCE_OK);

    Require(scePthreadKeyCreate(&sharedKey, nullptr) == SCE_OK);
    const PthreadKey held = sharedKey;
    Require(scePthreadCreate(&thread, nullptr, HoldAcrossReuse, nullptr, nullptr) == SCE_OK);
    WaitFor(1);
    Require(scePthreadKeyDelete(sharedKey) == SCE_OK);
    Require(scePthreadKeyCreate(&sharedKey, nullptr) == SCE_OK);
    Require(sharedKey == held);
    stage = 2;
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(scePthreadGetspecific(sharedKey) == nullptr);
}
