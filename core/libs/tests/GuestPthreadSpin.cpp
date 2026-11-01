#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdlib>
#include <stdexcept>

using PthreadSpinlock = PthreadSpinlockPrivate*;

static constexpr int GUEST_EPERM = 1;
static constexpr int GUEST_EBUSY = 16;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GuestPthread_Process_Private = 0;
static constexpr int GuestPthread_Process_Shared = 1;
static constexpr int Increments = 200000;

static void Require(bool value) { if (!value) std::abort(); }

struct Shared {
    PthreadSpinlock lock = nullptr;
    long counter = 0;
    int trylock = -1;
    int unlock = -1;
};

static void* APS5_VABI Contender(void* arg) {
    auto& shared = *static_cast<Shared*>(arg);
    shared.trylock = pthread_spin_trylock_nid_postfix(&shared.lock);
    shared.unlock = pthread_spin_unlock_nid_postfix(&shared.lock);
    return nullptr;
}

static void* APS5_VABI Incrementer(void* arg) {
    auto& shared = *static_cast<Shared*>(arg);
    for (int i = 0; i < Increments; ++i) {
        Require(pthread_spin_lock_nid_postfix(&shared.lock) == 0);
        ++shared.counter;
        Require(pthread_spin_unlock_nid_postfix(&shared.lock) == 0);
    }
    return nullptr;
}

static void ArgumentErrors() {
    Require(pthread_spin_init_nid_postfix(nullptr, GuestPthread_Process_Private) == GUEST_EINVAL);
    PthreadSpinlock lock = nullptr;
    Require(pthread_spin_init_nid_postfix(&lock, GuestPthread_Process_Shared) == GUEST_EINVAL);
    Require(pthread_spin_init_nid_postfix(&lock, 2) == GUEST_EINVAL);
    Require(pthread_spin_init_nid_postfix(&lock, -1) == GUEST_EINVAL);
    Require(lock == nullptr);
    Require(pthread_spin_lock_nid_postfix(nullptr) == GUEST_EINVAL);
    Require(pthread_spin_trylock_nid_postfix(nullptr) == GUEST_EINVAL);
    Require(pthread_spin_unlock_nid_postfix(nullptr) == GUEST_EINVAL);
    Require(pthread_spin_destroy_nid_postfix(nullptr) == GUEST_EINVAL);
    Require(pthread_spin_lock_nid_postfix(&lock) == GUEST_EINVAL);
    Require(pthread_spin_trylock_nid_postfix(&lock) == GUEST_EINVAL);
    Require(pthread_spin_unlock_nid_postfix(&lock) == GUEST_EINVAL);
    Require(pthread_spin_destroy_nid_postfix(&lock) == GUEST_EINVAL);
}

static void SingleThread(int pshared) {
    PthreadSpinlock lock = nullptr;
    Require(pthread_spin_init_nid_postfix(&lock, pshared) == 0);
    Require(lock != nullptr);
    Require(pthread_spin_unlock_nid_postfix(&lock) == GUEST_EPERM);
    Require(pthread_spin_lock_nid_postfix(&lock) == 0);
    Require(pthread_spin_trylock_nid_postfix(&lock) == GUEST_EBUSY);
    bool threw = false;
    try {
        pthread_spin_lock_nid_postfix(&lock);

    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    Require(pthread_spin_unlock_nid_postfix(&lock) == 0);
    Require(pthread_spin_unlock_nid_postfix(&lock) == GUEST_EPERM);
    Require(pthread_spin_trylock_nid_postfix(&lock) == 0);
    Require(pthread_spin_unlock_nid_postfix(&lock) == 0);
    Require(pthread_spin_destroy_nid_postfix(&lock) == 0);
    Require(lock == nullptr);
    Require(pthread_spin_destroy_nid_postfix(&lock) == GUEST_EINVAL);
}

static void HeldByAnotherThread() {
    Shared shared;
    Require(pthread_spin_init_nid_postfix(&shared.lock, GuestPthread_Process_Private) == 0);
    Require(pthread_spin_lock_nid_postfix(&shared.lock) == 0);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Contender, &shared, nullptr) == 0);
    Require(scePthreadJoin(thread, nullptr) == 0);
    Require(shared.trylock == GUEST_EBUSY);
    Require(shared.unlock == GUEST_EPERM);
    Require(pthread_spin_unlock_nid_postfix(&shared.lock) == 0);
    Require(pthread_spin_destroy_nid_postfix(&shared.lock) == 0);
}

static void MutualExclusion() {
    Shared shared;
    Require(pthread_spin_init_nid_postfix(&shared.lock, GuestPthread_Process_Private) == 0);
    Pthread first = nullptr;
    Pthread second = nullptr;
    Require(scePthreadCreate(&first, nullptr, Incrementer, &shared, nullptr) == 0);
    Require(scePthreadCreate(&second, nullptr, Incrementer, &shared, nullptr) == 0);
    Require(scePthreadJoin(first, nullptr) == 0);
    Require(scePthreadJoin(second, nullptr) == 0);
    Require(shared.counter == 2L * Increments);
    Require(pthread_spin_destroy_nid_postfix(&shared.lock) == 0);
}

int main() {
    ArgumentErrors();
    SingleThread(GuestPthread_Process_Private);
    HeldByAnotherThread();
    MutualExclusion();
}
