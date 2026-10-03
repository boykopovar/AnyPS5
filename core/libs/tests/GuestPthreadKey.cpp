#include "SceTypes.hpp"
#include <atomic>
#include <cstdlib>
#include <thread>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadKeyCreate(PthreadKey* key, pthread_key_destructor_func_t destructor);
int APS5_VABI scePthreadKeyDelete(PthreadKey key);
void* APS5_VABI scePthreadGetspecific(PthreadKey key);
int APS5_VABI scePthreadSetspecific(PthreadKey key, void* value);
}

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);

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
    PthreadKey key = -1;
    Require(scePthreadKeyCreate(&key, nullptr) == SCE_OK);
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
