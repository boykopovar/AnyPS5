#include "SceTypes.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <thread>

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
Pthread APS5_VABI scePthreadSelf(void);
int APS5_VABI pthread_join_nid_postfix(Pthread thread, void** retval);
int APS5_VABI scePthreadDetach(Pthread thread);
}

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EDEADLK = static_cast<int>(0x8002000Bu);
static constexpr int GUEST_EDEADLK = 11;

static void Require(bool value) { if (!value) std::abort(); }

static void* APS5_VABI JoinSelf(void*) {
    void* result = reinterpret_cast<void*>(std::uintptr_t{0x55});
    Require(scePthreadJoin(scePthreadSelf(), &result) == SCE_KERNEL_ERROR_EDEADLK);
    Require(pthread_join_nid_postfix(scePthreadSelf(), &result) == GUEST_EDEADLK);
    Require(result == reinterpret_cast<void*>(std::uintptr_t{0x55}));
    return reinterpret_cast<void*>(std::uintptr_t{0x1234});
}

static std::atomic<int> detachedChecks{0};

static void* APS5_VABI DetachThenJoinSelf(void*) {
    void* result = reinterpret_cast<void*>(std::uintptr_t{0x66});
    Require(scePthreadDetach(scePthreadSelf()) == SCE_OK);
    Require(scePthreadJoin(scePthreadSelf(), &result) == SCE_KERNEL_ERROR_EDEADLK);
    Require(pthread_join_nid_postfix(scePthreadSelf(), &result) == GUEST_EDEADLK);
    Require(result == reinterpret_cast<void*>(std::uintptr_t{0x66}));
    detachedChecks.store(1);
    return nullptr;
}

int main() {
    Pthread thread = nullptr;
    void* result = nullptr;
    Require(scePthreadCreate(&thread, nullptr, JoinSelf, nullptr, "join-self") == SCE_OK);
    Require(scePthreadJoin(thread, &result) == SCE_OK);
    Require(result == reinterpret_cast<void*>(std::uintptr_t{0x1234}));

    PthreadAttr unset = nullptr;
    thread = nullptr;
    result = nullptr;
    Require(scePthreadCreate(&thread, &unset, JoinSelf, nullptr, "unset-attr") == SCE_OK);
    Require(scePthreadJoin(thread, &result) == SCE_OK);
    Require(result == reinterpret_cast<void*>(std::uintptr_t{0x1234}));

    thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, DetachThenJoinSelf, nullptr, "detach-join-self") == SCE_OK);
    for (int wait = 0; wait < 5000 && detachedChecks.load() == 0; ++wait) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Require(detachedChecks.load() == 1);

    result = reinterpret_cast<void*>(std::uintptr_t{0x77});
    Require(scePthreadJoin(scePthreadSelf(), &result) == SCE_KERNEL_ERROR_EDEADLK);
    Require(pthread_join_nid_postfix(scePthreadSelf(), &result) == GUEST_EDEADLK);
    Require(result == reinterpret_cast<void*>(std::uintptr_t{0x77}));
}
